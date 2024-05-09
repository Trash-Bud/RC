/*Non-Canonical Input Processing*/
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include<string.h>
#include "../utils.c"

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

int n_seq = 0;

extern int Nr;
extern int Ns;

extern int alarm_count;
extern unsigned alarm_is_inactive;

void atende()
{
	printf("alarme # %d\n", alarm_count);
	alarm_count++;
  alarm_is_inactive = 1;
}

int control_packet(char* buf, char control, int l_size,char* size, char l_name, char* name){

  int i = 0, j =0;

  buf[0] = control;
  buf[1] = SIZE;
  buf[2] = (char)l_size;
  
  for ( ;i< l_size; i++){
    buf[3 + i] = size[i];
  }

  buf[3+i] = NAME;
  buf[3+i+1] = l_name;

  for ( ;j< (int)l_name; j++){
    buf[5 + i + j] = name[j];
  }

  return (5 + i + j);
}

int data_packet(char* buf, char* data, int data_size){
  int i = 0;
  char size[9] = {};

  buf[0] = DATA;
  buf[1] = (n_seq % 255);
  n_seq ++;

  size[0] = (unsigned char) data_size/256;
  size[1] = (unsigned char) data_size%256;

  buf[2] = size[0];
  buf[3] = size[1];
  
  for ( ;i< data_size; i++){
    buf[4 + i] = data[i];
  }
  return (4 + i);
}

int trama_dados(char *data, char* packet, int packet_size){
    int i = 0;
    char bcc = packet[0];

    data[0] = FLAG; 
    data[1] = TRANSMITER_ADRESS;
    data[2] = S(Ns);
    Ns = !Ns;
    data[3] = data[1] ^ data[2];

    for (;i<packet_size;i++){
      if (i != 0) bcc = bcc ^ packet[i];
      data[4+i] = packet[i];

    }
    data[4+i] = bcc;
    data[4+i+1] = FLAG;

    return 4+i+2;
}

void add_item_in_array_pos(char *buf,char bit, int pos, int length){
  char *buffer = malloc(length + 1);
  memcpy(buffer, buf, length);
  buf[pos] = bit;
  for (int i = pos+1; i<(length+1); i++){
    buf[i] = buffer[i-1];
  }
  free(buffer);
}

int stuffing(char* trama, int length){
  int a = 0;
  for (int i = 1; i<(length-1); i++){
    if (trama[i] == FLAG || trama[i] == ESCAPE){
      trama[i] = trama[i] ^ 0x20;
      add_item_in_array_pos(trama,ESCAPE,i,length+a);
      a++;
    }
  }
  return length + a;
}

int llwrite(int fd, char* data, int length){ 
    int res;
    int l;
    int count_bytes = 0;
    char temp[255], read_msg[255] = {};

    char *data_trama = malloc(((length + 6)*2)*sizeof(char));
    l = trama_dados(data_trama,data,length);   

    l = stuffing(data_trama,l);
    
    

    while (STOP==FALSE) {     
      if (alarm_count < 4 && alarm_is_inactive) {
        tcflush(fd, TCOFLUSH);
        res = write(fd,data_trama,l); 
        printHex(data_trama,l);
        printf("sent Data\n"); 

        alarm(3);
        alarm_is_inactive = 0;
      }
      else if (alarm_count >= 4 && alarm_is_inactive) { //if time-out
        free(data_trama);
        return -1; 
      }
      
      
      //read response
      res = read(fd,temp,1);   
      //printf("%d",res);

      if (res == 1){ //If recieved 1 byte then assemble trama
        read_msg[count_bytes] = temp[0];
        count_bytes++;
        if (read_msg[0] != FLAG) count_bytes = 0;

        if (count_bytes == 5) { //verify if response is correct
          printHex(read_msg,5);
          count_bytes = 0;
          if (ver_trama_sup(read_msg,RR(Nr),RECEIVER_ADRESS)){
              printf("Recieved RR\n");
              Nr = !Nr;
              //Deactivating and resetting alarm
              alarm(0);
              alarm_is_inactive = 1;
              alarm_count = 1;
              count_bytes = 0;

              //Stop loop
              break;
          }
          else if (ver_trama_sup(read_msg,REJ(Nr),RECEIVER_ADRESS)){
              Nr = !Nr;
              count_bytes = 0;
              printf("Recieved REJ\n");
              alarm_is_inactive = 1;
          }  
        };
      }
    }
    
    free(data_trama);
    return l;
}

int llclose(int fd)
{
    send_sup_trama(DISC, fd);

    close_termios(fd);

    int r = close(fd);
    if (r < 0)
    {
        perror("Couldn't close");
        return (-1);
    }
    return 0;
}

int send_file(int fd, FILE* file, char* name){

  fseek(file, 0L, SEEK_END);
  int fileSize = ftell(file);
  fseek(file, 0L, SEEK_SET);

  char size[255] = {};

  int size_s = sprintf(size, "%x", fileSize);

  char control_p[255] = {};
  int length = control_packet(control_p,START,size_s,size,strlen(name),name);

  llwrite(fd,control_p,length);

  unsigned char *data = malloc(fileSize);

  fread(data,sizeof(unsigned char),fileSize,file);

  int current_data_sent = 0;

  while (current_data_sent < fileSize){
    printf("%d of %d\n",current_data_sent,fileSize);
    char packet[249] = {};
    char packet_data[245] = {};
    int i = 0;
    for (; i < 245 && current_data_sent + i < fileSize; i++){
      packet_data[i] = data[current_data_sent + i];
    }

    int length = data_packet(packet,packet_data,i);


    if(llwrite(fd, packet, length) == -1) return -1;
    current_data_sent += i;
  }
  printf("%d of %d\n",current_data_sent,fileSize);
  printf("finished sending file\n");
  length = control_packet(control_p,END,size_s,size,strlen(name),name);

  llwrite(fd,control_p,length);
  free(data);
  return 0;
}

int main(int argc, char** argv)
{
    (void) signal(SIGALRM, atende); 
    
    if ( (argc < 2) || 
         ((strcmp("/dev/ttyS10", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS11", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS1", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS0", argv[1])!=0)) ) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS11\n");
      exit(1);
    }

    FILE* file = fopen(argv[2],"rb");
    if (file == NULL){
      printf("Could not open file\n");
      exit(1);
    }

    int fd = llopen(argv[1], TRANSMITER);
    if (fd == -1){
      return -1;
    }

    if(send_file(fd,file,argv[2]) == -1) return -1;
      
    if (llclose(fd) == -1){
      return -1;
    }

    fclose(file);
    
    
    return 0;
}
