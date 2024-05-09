/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>

#include <string.h>

#define SERVER_PORT 21
#define SERVER_ADDR "192.168.28.96"

void getAddress(char* address){

    struct hostent *h;

    if ((h = gethostbyname(address)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    strcpy(address, inet_ntoa(*((struct in_addr *) h->h_addr)));
}

char* read_from_socket(FILE* fd, char* r){
    char* i = fgets(r,250,fd);
    printf("%s",r);
    return i;
}

int get_termB_port(FILE* fd, char* message){
    int num = 0, count=0;
    char* num2 = (char *) malloc(3); 
    char* num1 = (char *) malloc(3); 
    int e;
    for (int i = 0; i <strlen(message);i++){
        if (message[i] == '('){
            num = 1;
        }
        if (num == 1){
            if (message[i] == ')'){
                break;
            }
            if (message[i] == ','){
                count ++;
                i++;
                e= 0;
            }
            if (count == 4){
                num1[e] = message[i];
                
                e++;
            }
            else if (count == 5){
                num2[e] = message[i];
                e++;
            }

        }
    }

    int port = atoi(num1) * 256 + atoi(num2);
    free(num1);
    free(num2);

    return port;
}

//ftp://anonymous:qualquer-password@ftp.up.pt:21/pub/kodi/timestamp.txt

void parseinput(char* input,int size, char* password, char* user, char* address, char* port, char* path){
    int i = 0;

    user[0] ='u';
    user[1] ='s';
    user[2] ='e';
    user[3] ='r';
    user[4] =' ';

    for (int e = 5; i < size; i++, e++){
        if (input[i] == ':'){
            user[e] = '\n';
            i++;
            break;
        }
        user[e] = input[i];
    }

    password[0] ='p';
    password[1] ='a';
    password[2] ='s';
    password[3] ='s';
    password[4] =' ';

    for (int e = 5; i < size; i++, e++){
        if (input[i] == '@'){
            password[e] = '\n';
            i++;
            break;
        }
        password[e] = input[i];
    }


    for (int e = 0; i < size; i++, e++){
        if (input[i] == ':'){
            i++;
            break;
        }
        address[e] = input[i];
    }

    for (int e = 0; i < size; i++, e++){
        if (input[i] == '/'){
            i++;
            break;
        }
        port[e] = input[i];
    }

    path[0] ='r';
    path[1] ='e';
    path[2] ='t';
    path[3] ='r';
    path[4] =' ';
    
    for (int e = 5; i <= size; i++, e++){
        if (i == size){
            path[e] = '\n';
        }
        else path[e] = input[i];
    }


    /*printf("user : %s\n",user);
    printf("password: %s\n",password);
    printf("address: %s\n",address);
    printf("port: %s\n",port);
    printf("path: %s\n",path);*/

}

int write_to_socket(int sockfd, char* str){
    int bytes;
    int count = strlen(str);

    bytes = write(sockfd, str, count);

    if (bytes > 0)
        printf("%s",str);
    else {
        perror("write()");
        return(-1);
    }

    return 0;
}

int close_sockets(int sockfd,int sockfd2){

    if (close(sockfd2)<0) {
        perror("close()");
        return(-1);
    }
    
    if (close(sockfd)<0) {
        perror("close()");
        return(-1);
    }
}

int main(int argc, char **argv) {

    char big[] = "ftp://";
    char input[255];
    char read[250];

    if (argc != 2 || strncmp(big, argv[1], 6) != 0) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>:<port>/<url-path>\n", argv[0]);
        exit(-1);
    }

    strcpy(input,argv[1]+6);

    int size = (int)strlen(argv[1]);

    int sockfd, sockfd2;
    struct sockaddr_in server_addr;
    char address[250] = {}, passive[] = "pasv\n", user[250] = {},password[250] = {}, path[255]={}, port[255] = {};
    

    parseinput(input,size-6,password,user,address,port,path);

    getAddress(address);

    //server address handling
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address);    //32 bit Internet address network byte ordered
    server_addr.sin_port = htons(atoi(port));        //server TCP port must be network byte ordered 

    //open a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    //connect to the server
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    FILE* fdread = fdopen(sockfd,"r");

    for (int i = 0; i <1;i++){
        read_from_socket(fdread,read);
    }

    
    write_to_socket(sockfd, user);
    read_from_socket(fdread,read);
    
    if (strncmp (read, "530 Permission denied.",22) == 0){
        if(close_sockets(sockfd,sockfd2) == -1){
            exit(-1);
        }
        return 0;
    }

    write_to_socket(sockfd, password);
    read_from_socket(fdread,read);

    if (strncmp (read, "530 Login incorrect.",20) == 0){
        if(close_sockets(sockfd,sockfd2) == -1){
            exit(-1);
        }
        return 0;
    }

    write_to_socket(sockfd, passive);
    read_from_socket(fdread,read);

    int port2 = get_termB_port(fdread, read);

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address);    //32 bit Internet address network byte ordered
    server_addr.sin_port = htons(port2);        //server TCP port must be network byte ordered 

    //open a TCP socket
    if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    //connect to the server
    if (connect(sockfd2,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    FILE* fdread2 = fdopen(sockfd2,"r");
    
    write_to_socket(sockfd, path);
    read_from_socket(fdread,read);

    if (strncmp (read, "550 Failed to open file.",24) == 0){
        if(close_sockets(sockfd,sockfd2) == -1){
            exit(-1);
        }
        return 0;
    }
    
    printf("\ndata:\n");
    while (1){
        char* i = read_from_socket(fdread2,read);
        if (i == NULL) break;
    }
    printf("\n");

    if(close_sockets(sockfd,sockfd2) == -1){
        exit(-1);
    }
    return 0;
}


