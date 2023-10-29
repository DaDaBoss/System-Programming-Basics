#define _GNU_SOURCE // for sigaction
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

// параметры сервера
char *address = "127.0.0.1";
char *port = "32451";
char *logp = "/tmp/lab2.log";
int wait_time = 0;
int serverSocket;
volatile sig_atomic_t sigclose = 0;
// время с начала работы программы для SIGUSR1
struct timespec begin;
// функции
static void sighandler(int signum);
void optparse(int argc, char *argv[]);
void check_result(int res, char *msg)
{
    if (res < 0)
    {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}
void printtime()
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    printf("%02d%c%02d%c%d  [%02d:%02d:%02d]  ", tm.tm_mday, '.', tm.tm_mon + 1, '.', tm.tm_year + 1900,
           tm.tm_hour, tm.tm_min, tm.tm_sec);
}
size_t forks = 0;
void handle_fork(char *buffer);
char *strrev(char *str);

int main(int argc, char *argv[])
{
    // begin = clock(); clock не сигнало безопасная, надо использовать clock_gettime
    check_result(clock_gettime(CLOCK_MONOTONIC, &begin), "clock_gettime()");
    // signals
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);

    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);

    sa.sa_handler = sighandler;
    sa.sa_flags |= (SA_RESTART);

    check_result(sigaction(SIGTERM, &sa, NULL), "sigaction(SIGTERM)");
    check_result(sigaction(SIGINT, &sa, NULL), "sigaction(SIGINT)");
    check_result(sigaction(SIGQUIT, &sa, NULL), "sigaction(SIGQUIT)");
    check_result(sigaction(SIGUSR1, &sa, NULL), "sigaction(SIGUSR1)");

    if (getenv("LAB2WAIT"))
        wait_time = strtol(getenv("LAB2WAIT"), NULL, 10);
    if (getenv("LAB2LOGFILE"))
        logp = getenv("LAB2LOGFILE");
    if (getenv("LAB2ADDR"))
        address = getenv("LAB2ADDR");
    if (getenv("LAB2PORT"))
        port = getenv("LAB2PORT");

    optparse(argc, argv);

    if(!freopen(logp, "w", stdout) || !freopen(logp, "w", stderr)){
        fprintf(stderr, "freopen() failed\n");
        exit(EXIT_FAILURE);
    }
    if (getenv("LAB2DEBUG"))
        fprintf(stderr, "Debug mode enabled\n");

    char buffer[1024] = {0};
    struct sockaddr_in serverAddr = {0};
    struct sockaddr_in clientAddr = {0};
    socklen_t addr_size;
    (void)addr_size;
    int res;

    // открываем сокет и выставляем нужные настройки
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    check_result(serverSocket, "socket");
    // reuseaddr чтобы сокет освобождался сразу же после закрытия
    res = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    check_result(res, "setsockopt");
    uint16_t intport = strtol(port, NULL, 10);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(intport);
    serverAddr.sin_addr.s_addr = inet_addr(address);
    // serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    res = bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    check_result(res, "bind");

    while (sigclose == 0)
    {
        errno = 0;
        if (forks > 0){
            (void)waitpid(-1, NULL, WNOHANG);
            if(errno == ECHILD) errno = 0;
        }
        addr_size = sizeof(clientAddr);
        res = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &addr_size);
        if(res < 0 && errno == EBADF && sigclose != 0) break;
        check_result(res, "recvfrom");
        if (res > 0)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "fork() failed\n");
                exit(EXIT_FAILURE);
                break;
            }
            else if (pid == 0)
            {
                printtime();
                printf("Data received (%d bytes): [%s] from %s:%d\n", (int)res, buffer,
                       inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                sleep(wait_time);
                if (res < 1024)
                    buffer[res] = '\0';

                handle_fork(buffer);
                printtime();
                printf("Sending: %s", buffer);

                res = sendto(serverSocket, buffer, strlen(buffer), 0,
                             (const struct sockaddr *)&clientAddr, addr_size);
                check_result(res, "sendto");
                
                exit(EXIT_SUCCESS);
            } else forks++;
        }
    }

    // close(serverSocket);
    while (waitpid(-1, NULL, 0) != -1)
        continue;
    check_result(fclose(stdout), "fclose(stdout)");
    check_result(fclose(stderr), "fclose(stderr)");
    return 0;
}

void printhelp()
{
    printf("UDP Server. Available options:\n"
           "--help (-h)\t\tPrint this help message\n"
           "--version (-v)\t\tPrint version\n"
           "--wait (-w)\t\tWait time in forked process\n"
           "--daemon (-d)\t\tDaemon mode"
           "--log (-l) /path/to/log\t\tLog file path (default - /tmp/lab2.log)\n"
           "--address (-a) ip\t\tListening address\n"
           "--port (-p) port\t\tListening port.\n");
    printf("Environment variables:\n"
           "LAB2DEBUG\t\tPrint additional debug info\n"
           "LAB2WAIT\t\tWait time in forked process\n"
           "LAB2LOGFILE\t\tPath to log file (default - /tmp/lab2.log)\n"
           "LAB2ADDR\t\tListening address\n"
           "LAB2PORT\t\tListening port\n\n");
}

void optparse(int argc, char *argv[])
{
    struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'v'},
        {"wait", required_argument, 0, 'w'},
        {"daemon", 0, 0, 'd'},
        {"log", required_argument, 0, 'l'},
        {"address", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}};
    int option_index = 0;
    int choice;
    while ((choice = getopt_long(argc, argv, "hvw:dl:a:p:", long_options, &option_index)) != -1)
    {
        switch (choice)
        {
        case 'h':
            printhelp();
            break;
        case 'v':
            printf("Smaglyuk Vladislav\nN32451\nVersion 0.1\n");
            break;
        case 'w':
            wait_time = strtol(optarg, NULL, 10);
            break;
        case 'd':
            if (daemon(0, 1) == -1)
            {
                fprintf(stderr, "daemon() error: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;
        case 'l':
            logp = optarg;
            break;
        case 'a':
            address = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        default:
            fprintf(stderr, "Unknown opt!\n");
            exit(EXIT_FAILURE);
        }
    }
}
void handle_fork(char *buffer)
{
    size_t buflen = strlen(buffer);
    // если меньше 11 символов то там не может быть 6 чисел
    if (buflen < 11)
    {
        sprintf(buffer, "ERROR %d\n", EINVAL);
        return;
    }
    for (size_t i = 0; i < buflen; i++)
    {
        if (buffer[i] == '\t')
            buffer[i] = ' ';
    }
    double arr[6] = {0};
    char *endptr1 = {0};
    char *endptr2 = {0};
    int i = 0;
    do
    {
        if (i == 0)
        {
            arr[i] = strtod(buffer, &endptr1);
            if (arr[i] == 0 && buffer == endptr1)
            {
                // fprintf(stderr, "strtod() failed on first num\n");
                if (errno != 0)
                    sprintf(buffer, "ERROR %d\n", errno);
                else
                    sprintf(buffer, "ERROR %d\n", EINVAL);
                return;
            }
        }
        else
        {
            arr[i] = strtod(endptr1, &endptr2);
            if (arr[i] == 0 && buffer == endptr1)
            {
                // fprintf(stderr, "strtod() failed on other nums\n");
                if (errno != 0)
                    sprintf(buffer, "ERROR %d\n", errno);
                else
                    sprintf(buffer, "ERROR %d\n", EINVAL);
                return;
            }
            endptr1 = endptr2;
        }
        i++;
    } while (i < 6);
    double x = strtod(endptr1, &endptr2);
    if ((endptr2 != endptr1) || x != 0)
    {
        // если слишком много чисел
        if (errno != 0)
            sprintf(buffer, "ERROR %d\n", errno);
        else
            sprintf(buffer, "ERROR %d\n", EINVAL);
        return;
    }
    errno = 0;
    //[0] = x1 [2] = x2 [4] = x3
    //[1] = y1 [3] = y2 [5] = y3
    double len1 = sqrt(pow(arr[2] - arr[0], (double)2) + pow(arr[3] - arr[1], (double)2));
    double len2 = sqrt(pow(arr[4] - arr[2], (double)2) + pow(arr[5] - arr[3], (double)2));
    double len3 = sqrt(pow(arr[4] - arr[0], (double)2) + pow(arr[5] - arr[0], (double)2));
    if (errno != 0)
    {
        sprintf(buffer, "ERROR %d\n", errno);
        return;
    }
    else
    {
        sprintf(buffer, "%f\n", len1 + len2 + len3);
    }
}
static void sighandler(int signum)
{
    if (signum == SIGUSR1)
    {
        write(STDOUT_FILENO, "SIGUSR1 Received.\nRuntime: ", 27);
        // выводим сколько времени работает
        struct timespec finish;
        if (clock_gettime(CLOCK_MONOTONIC, &finish) < 0)
            _exit(EXIT_FAILURE);
        long runtime = finish.tv_sec - begin.tv_sec;
        char buf[128];
        int i = 0;
        //sprintf и printf несигналобезопасные, нельзя использовать при обработке сигналов
        //поэтому конвертируем число в строку так
        while (runtime > 0)
        {
            buf[i] = (runtime % 10) + '0';
            runtime = runtime / 10;
            i++;
        }
        buf[i] = '\0';
        //реверс строки из примера Гирика
        char *p1, *p2;
        for (p1 = buf, p2 = buf + strlen(buf) - 1; p2 > p1; ++p1, --p2)
        {
            *p1 ^= *p2;
            *p2 ^= *p1; //побитовый xor чтобы реверснуть строку
            *p1 ^= *p2;
        }
        write(STDOUT_FILENO, buf, strlen(buf));
        write(STDOUT_FILENO, " sec.\n", 7);
        write(STDOUT_FILENO, "Forks:", 7);
        size_t forks_copy = forks;
        int j = 0;
        while (forks_copy > 0)
        {
            buf[j] =(forks_copy%10) + '0';
            forks_copy = forks_copy/10;
            j++;
        }
        buf[j]='\0';
        for (p1 = buf, p2 = buf + strlen(buf) - 1; p2 > p1; ++p1, --p2)
        {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
        }
        write(STDOUT_FILENO, buf, strlen(buf));
        write(STDOUT_FILENO, "\n", 1);
    }
    else if (signum == SIGTERM || signum == SIGQUIT || signum == SIGINT)
    {
        sigclose = 1;
        close(serverSocket);
    }
}