/*Non-Canonical Input Processing*/
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../utils.c"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

extern int Nr;
extern int Ns;
int n_seq_ant = -1;

void remove_item_in_array_pos(char *buf, int pos, int length){
  char *buffer = malloc(length);
  memcpy(buffer, buf, length);
  for (int i = pos; i<(length-1); i++){
    buf[i] = buffer[i+1];
  }
  free(buffer);
}

int ver_trama_dados(char *buf){
  int i = 5;
  int bcc = buf[4];

  while (buf[i+1] != FLAG){
    if (buf[i] == ESCAPE){
      bcc = bcc ^ (buf[i+1] ^ 0X20);
      i+= 2;
    }
    else{
      bcc = bcc ^ buf[i];
      i++;
    }
  }

  //printf("bcc : %02X\n",bcc);

  return (bcc == buf[i]);
}

void get_info_control(char* packet,char* name, int* size){
  int i = 0, j = 0;

  if (packet[1] == SIZE){
    int length = (int) packet[2];
    char size1[255] = {};
    for (; i< length;i++){
      size1[i] = packet[3 + i];
    }
    *size = (int)strtol(size1, NULL, 16);
    if (packet[3+i] == NAME){
      length = (int) packet[3+i+1];
      for (; j< length;j++){
        name[j] = packet[3 + i +2 +j];
      }
    }
  }
}

int remove_data_packet(char* packet, char* data){
  int size = 256 * (int)(unsigned char)packet[2] + (int)(unsigned char)packet[3];
  for (int i = 0 ;i< size; i++){
    data[i] = packet[4+i];
  }
  return (size);
}

int remove_head(char *data, char* packet){
    int i = 0;

    while (data[4+i+1]!= FLAG){
      packet[i] = data[4+i];
      i++;
    }
    //printf("%d\n",i);
    return i;
}

int destuffing(char* trama, int length){ 
  int a = 0;
  for (int i = 1; i<(length-1); i++){
    if (trama[i] == ESCAPE){
      remove_item_in_array_pos(trama,i,length - a);
      trama[i] = trama[i] ^ 0x20;
      a++;
    }
  }
  return (length -a);
}

int llread(int fd, char* buffer){
    int length, packet_length;
    int write_res, res;
    char read_buffer[510] = {}, write_buffer[5], temp_read[1] = {};
    char packet[510] = {};
    int flag_num = 0;
    int read_buf_last = 0;

    while (STOP==FALSE) {  
      res = read(fd,temp_read,1);
      //printHex(temp_read, 1);
      if (temp_read[0] == FLAG) flag_num++;
      read_buffer[read_buf_last] = temp_read[0];
      read_buf_last++;
      if (res == -1){ 
        perror("Incorrect read\n");
        return -1;
      }

      printf("read data\n");          
      if (flag_num >= 2) printHex(read_buffer, 510);
      if (ver_trama_head(read_buffer, S(Ns),TRANSMITER_ADRESS) && flag_num >= 2){
        flag_num = 0; 
        read_buf_last = 0; 
          if (ver_trama_dados(read_buffer)){
            trama_set(write_buffer,RECEIVER_ADRESS,RR(Nr));
            Nr = !Nr;
            printf("sent RR\n");
            tcflush(fd,TCOFLUSH);
            if ((write_res = write(fd,write_buffer,5)) == -1) return -1;

            packet_length = remove_head(read_buffer,packet);       

            length = destuffing(packet, packet_length);

            for (int i = 0; i< packet_length; i++){
              buffer[i] = packet[i];
            }
            Ns = !Ns;
            return length;
          }
          else{
            printf("data is incorrect!\n");
            trama_set(write_buffer,RECEIVER_ADRESS,REJ(Nr));
            Nr = !Nr;
            tcflush(fd,TCOFLUSH);
            if ((write_res = write(fd,write_buffer,5)) == -1) return -1;
            printf("sent REJ\n");
          }
      }else if (ver_trama_head(read_buffer, S(!Ns),TRANSMITER_ADRESS) && flag_num >= 2){
        flag_num = 0;  
        read_buf_last = 0;
        if (!ver_trama_dados(read_buffer)){
          printf("data is incorrect!\n");
            trama_set(write_buffer,RECEIVER_ADRESS,REJ(Nr));
            Nr = !Nr;
            tcflush(fd,TCOFLUSH);
            if ((write_res = write(fd,write_buffer,5)) == -1) return -1;
            printf("sent REJ\n");
        }
        else{
          trama_set(write_buffer,RECEIVER_ADRESS, RR(Nr));
          Nr = !Nr;
          printHex(write_buffer, 5);
          tcflush(fd,TCOFLUSH);
          if ((write_res = write(fd,write_buffer,5)) == -1) return -1;
          printf("trama was repeated ^^! Sent RR\n");
        }
    }
    else if (read_buffer[0] != FLAG){
        flag_num = 0;  
        read_buf_last = 0;

    }
     
    }
}

int llclose(int fd)
{
    read_sup_trama(fd);
    read_sup_trama(fd);
  
    close_termios(fd);

    int r = close(fd);
    if (r < 0)
    {
        perror("Couldn't close");
        return (-1);
    }
    return 0;
}

int get_file(int fd){

  char control_p[255] = {};

  int length = llread(fd,control_p);
  if (length ==  -1) {
    return -1;
  }

  int size;
  char name[255] = {};

  if (control_p[0] == START){
    get_info_control(control_p,name,&size);
    printf("size: %d, name: %s\n",size,name);
  }

  FILE* file = fopen(name,"wb");
  if (file == NULL){
    printf("Could not open file\n");
    return -1;
  }

  char packet[249] ={};
  while (TRUE){
    length = llread(fd,packet);
    if (length ==  -1) {
      return -1;
    }
    if (packet[0] == END) {
      int size1;
      char name1[255] = {};
      get_info_control(control_p,name1,&size1);
      if (strcmp(name,name1) == 0 && size == size1) break;
    }
    else if (packet[1] != n_seq_ant){
      n_seq_ant = packet[1];
      char packet_data[245] = {};
      length = remove_data_packet(packet,packet_data);

      int res_w = fwrite(packet_data,sizeof(unsigned char),length,file);
      if (res_w == -1){
        return -1;
      }
      printf("wrote: %d bytes\n", res_w); 
    }  
  }

  fclose(file); 
  return 0;
}

int main(int argc, char** argv)
{

  if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS10", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS11", argv[1])!=0) &&
          (strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

    int fd = llopen(argv[1], RECEIVER);
    if (fd == -1){
      return(-1);
    }

    if (get_file(fd) == -1) return -1;

    llclose(fd);
    
    return 0;
}
