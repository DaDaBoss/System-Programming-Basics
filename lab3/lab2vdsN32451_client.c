#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

void check_result(int res, char *msg){
    if(res < 0){
        perror(msg);
        exit(EXIT_FAILURE);
    }
}
int data = 0;
char *address = "127.0.0.1";
char *port = "32451";
void optparse(int argc, char *argv[]);

int main(int argc, char* argv[]) {

    if(getenv("LAB2ADDR")) address = getenv("LAB2ADDR");
    if(getenv("LAB2PORT")) port = getenv("LAB2PORT");
    
    optparse(argc, argv);
	int clientSocket;
	char buffer[1024] = {0};
	struct sockaddr_in serverAddr = {0};
	socklen_t addr_size = sizeof(serverAddr);

	clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
	check_result(clientSocket, "socket");
	
	serverAddr.sin_family = AF_INET;
    uint16_t intport = strtol(port, NULL, 10);
	serverAddr.sin_port = htons(intport);
	serverAddr.sin_addr.s_addr = inet_addr(address);
    if(!data)printf("Data format: x1 y1 x2 y2 x3 y3 (space or tab between)\n");
	printf("Data to send: ");
	fgets(buffer, sizeof(buffer), stdin);
	char *pos = strchr(buffer, '\n');
	if (pos) *pos = '\0';
	
	int res = sendto(clientSocket, buffer, strlen(buffer), 0, 
		(const struct sockaddr*)&serverAddr, addr_size);
	check_result(res, "sendto");

	memset(buffer, 0, sizeof(buffer));
		
	res = recvfrom(clientSocket, buffer, sizeof(buffer), 0, NULL, NULL);
	check_result(res, "recvfrom");
	
	printf("Data received (%d bytes): [%s]\n", (int)res, buffer);   

	close(clientSocket);
	
	return 0;
}

void printhelp(){
    printf( "UDP Client. Available options:\n"
            "--help (-h)\t\tPrint this help message\n"
            "--version (-v)\t\tPrint version\n"
            "--address (-a) ip\t\tSending address\n"
            "--port (-p) port\t\tSending port.\n"
            "--data (-d) \t\tData to send");
    printf( "Environment variables:\n"
            "LAB2DEBUG\t\tPrint additional debug info\n"
            "LAB2ADDR\t\tSending address\n"
            "LAB2PORT\t\tSending port\n\n");
}

void optparse(int argc, char *argv[]){
    struct option long_options[]={
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'v'},
        {"address", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {0,0,0,0}
    };
    int option_index = 0;
    int choice;
    while ((choice = getopt_long(argc, argv, "hva:p:", long_options, &option_index)) != -1)
    {
        switch (choice)
        {
            case 'h':
            printhelp();
            break;
            case 'v':
            printf("Smaglyuk Vladislav\nN32451\nVersion 0.1\n");
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
