#include <termios.h>

#define SET 0x03
#define DISC 0x0B
#define UA 0x07
#define RR(nr) (nr? 0x085: 0x05)
#define REJ(nr) (nr? 0x081 : 0x01)
#define S(ns) (ns? 0x40:0x00)

#define FLAG 0x7E
#define ESCAPE 0x7D
#define RECEIVER_ADRESS 0x01
#define TRANSMITER_ADRESS 0x03
#define SIZE 0x00
#define NAME 0x01
#define DATA 1
#define START 2
#define END 3
#define RECEIVER 1
#define TRANSMITER 0

#define BAUDRATE B38400
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int Nr = 1;
int Ns = 0;
struct termios oldtio;
int alarm_count = 1;
unsigned alarm_is_inactive = 1;

int ver_trama_head(char *buf, char control, int adress)
{
    return (buf[0] == FLAG && buf[1] == adress && buf[2] == control && buf[3] == (buf[1] ^ buf[2]));
}

int ver_trama_sup(char *buf, char control, int adress)
{
    return (ver_trama_head(buf, control, adress) && buf[4] == FLAG);
}

void printHex(char* buffer,int length){
    for (int i = 0; i<length;i++){
        printf("%02x",buffer[i]);
    }
    printf("\n");
    printf("\n");
}


void trama_set(char *buf, char address, char control)
{
    buf[0] = FLAG;
    buf[1] = address;
    buf[2] = control;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = FLAG;

}

int send_sup_trama(int control, int fd)
{
    char buf[255] = {};
    char read_msg[255] = {};
    char temp[255];
    int res;
    int count_bytes = 0;

    
    while (STOP == FALSE)
    {
        if (alarm_count < 4 && alarm_is_inactive)
        {
            //set trama and write
            trama_set(buf, TRANSMITER_ADRESS, control);
            res = write(fd, buf, 5);
            if (res == -1)
                return -1;

            //set alarm
            alarm(3);
            alarm_is_inactive = 0;
        }
        else if (alarm_count >= 4 && alarm_is_inactive)
            return -1; //if time-out

        //read response
        res = read(fd, temp, 1);
        

        if (res == 1)
        { //If recieved 1 byte then assemble trama
            read_msg[count_bytes] = temp[0];
            count_bytes++;

            if (count_bytes == 5)
            { //verify if response is correct
                if ((control == SET && ver_trama_sup(read_msg, UA, RECEIVER_ADRESS)))
                {
                    //Deactivating and resetting alarm
                    alarm(0);
                    alarm_is_inactive = 1;
                    alarm_count = 1;
                    printf("sent SET -> recieved UA\n");
                    
                    //Stop loop
                    
                    return 0;
                }
                else if ((control == DISC && ver_trama_sup(read_msg, DISC, RECEIVER_ADRESS)))
                {
                    printf("sent DISC -> recieved DISC\n");
                    trama_set(buf, TRANSMITER_ADRESS, UA);
                    res = write(fd, buf, 5);
                    printf("sent UA\n");
                    return 0;
                }
            }
        }
    }
}

int read_sup_trama(int fd)
{

    int write_res, res;
    char read_buffer[5] = {}, write_buffer[5];

    while (STOP==FALSE) {       
      res = read(fd,read_buffer,5);  
      if (res == -1){ 
        perror("Incorrect read\n");
        return -1;
      }           

      if (read_buffer[2] == DISC && ver_trama_sup(read_buffer,DISC,TRANSMITER_ADRESS)){
            trama_set(write_buffer,RECEIVER_ADRESS, DISC);
            printf("sent DISC\n");
            break;
  
      }
      else  if (read_buffer[2] == SET && ver_trama_sup(read_buffer,SET,TRANSMITER_ADRESS)){
            trama_set(write_buffer,RECEIVER_ADRESS, UA);
            printf("sent UA\n");
            break;
       }
       else if (read_buffer[2] == UA && ver_trama_sup(read_buffer,UA,TRANSMITER_ADRESS)){
           printf("recieved UA\n");
           return res;
       }
    } 
    write_res = write(fd,write_buffer,5);
    
    if (write_res  == -1){
        return -1;
    }

    return res;
}

int open_termios(int fd, int type)
{
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
            perror("tcgetattr");
            return (-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    if (type == TRANSMITER)
    {
        newtio.c_cc[VTIME] = 1;
        newtio.c_cc[VMIN] = 0;
    }
    else
    {
        newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
        newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */
    }

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return (-1);
    }

    printf("New termios structure set\n");

    return 0;
}

int llopen(char *port, int type)
{
    int fd;

    fd = open(port, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(port);
        return (-1);
    }

    open_termios(fd,type); 

    if (type == TRANSMITER)
    {
        send_sup_trama(SET,fd);
    }
    else{
        read_sup_trama(fd);
    }
    return fd;
}

int close_termios(int fd)
{
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        return (-1);
    }
    return 0;
}
