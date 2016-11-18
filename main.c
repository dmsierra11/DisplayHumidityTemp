/** Example c code for accessing Honeywell temperature+humidity sensors
*
* Author: Diego Vierma & Daniel Sierra
*
*
**/

#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <wiringPi.h>
#include <geniePi.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

int temperature = 10;
int alarma = 0;
time_t t;
struct tm *tm;

char* concat(int s1, int s2)
{
    char str1[30];
    char str2[30];
    const char *hum = "Humedad: ";
    const char *tem = "\nTemperatura: ";
    snprintf(str1, 10, "%d", s1);
    snprintf(str2, 10, "%d", s2);
    printf("STR1 %d", s1);
    char *result = malloc(strlen(str1)+strlen(str2)+strlen(hum)+strlen(tem)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, "Humedad: ");
    strcat(result, str2);
    strcat(result, "\nTemperatura: ");
    strcat(result, str1);
    return result;
}

static void *handleTime(void *data) {

    for(;;) {

        t = time (NULL);
        tm = localtime(&t);
        char s[30];
        strftime(s, sizeof(s), "%H%M", tm);

        char seg[30];
        strftime(seg, sizeof(seg), "%S", tm);

        int time = (int) strtol(s, (char **)NULL, 10);
        genieWriteObj(GENIE_OBJ_LED_DIGITS, 1, time);

        int seconds = (int) strtol(seg, (char **)NULL, 10);
        genieWriteObj(GENIE_OBJ_LED_DIGITS, 2, seconds);

    }

}

static void *handleAngularMeter(void *data) {

int fd;                                   /* File descriptor*/
const char *fileName = "/dev/i2c-1";  /* Name of the port we will be using. On Raspberry 2 this is i2c-1, on an older Raspberry Pi 1 this might be i2c-0.*/
int  address = 0x27;                  /* Address of Honeywell sensor shifted right 1 bit */
unsigned char buf[4];             /* Buffer for data read/written on the i2c bus */
char hum[30];
char tempo[30];

    for (;;) {

        if ((fd = open(fileName, O_RDWR)) < 0)
      {
        printf("Failed to open i2c port\n");
        exit(1);
      }

      /* Set port options and slave devie address */
      if (ioctl(fd, I2C_SLAVE, address) < 0)
      {
        printf("Unable to get bus access to talk to slave\n");
        exit(1);
      }


      /* Initiate measurement by sending a zero bit (see datasheet for communication pattern) */
      if ((i2c_smbus_write_quick(fd, 0)) != 0)
      {
        printf("Error writing bit to i2c slave\n");
        exit(1);
      }

      /* Wait for 100ms for measurement to complete.
         Typical measurement cycle is 36.65ms for each of humidity and temperature, so you may reduce this to 74ms. */
      usleep(100000);

      /* read back data */
      if (read(fd, buf, 4) < 0)
      {
        printf("Unable to read from slave\n");
        exit(1);
      }
      else
      {


            /* Humidity is located in first two bytes */
            int reading_hum = (buf[0] << 8) + buf[1];
            double humidity = reading_hum / 16382.0 * 100.0;
            int humedad = (int) humidity;
            printf("Humidity: %f\n", humidity);
            //sprintf(hum, "Humidity: %d", humidity);

            /* Temperature is located in next two bytes, padded by two trailing bits */
            int reading_temp = (buf[2] << 6) + (buf[3] >> 2);
            double temperature = reading_temp / 16382.0 * 165.0 - 40;
            int temperatura = (int) temperature;
            sprintf(tempo, "Temperature: %d", temperatura);
            printf("Temperature: %s\n\n", tempo);

            genieWriteObj(GENIE_OBJ_ANGULAR_METER, 0, temperatura);
            genieWriteObj(GENIE_OBJ_METER, 0, humedad);

            if (temperatura > alarma)
                genieWriteObj(GENIE_OBJ_USER_LED, 0, 1);
            else
                genieWriteObj(GENIE_OBJ_USER_LED, 0, 0);

            char* temp = concat(temperatura, humedad);
            genieWriteStr(0x00, temp);
            free(temp);
            usleep(500000);
     }
    }
    return NULL;
}

void handleGenieEvent (struct genieReplyStruct * reply)
{
    if(reply->cmd == GENIE_REPORT_EVENT)
    {
        if(reply->object == GENIE_OBJ_KNOB)
        {
            if(reply->index == 0) {
                alarma = reply->data;

                char temp2[30] = "prueba";
                snprintf(temp2, 10, "%d", alarma);
                genieWriteStr(0x01, temp2);

            }

        }
    }
}

int main(int argc, char **argv)
{

pthread_t threadSample;
pthread_t threadTiempo;
struct genieReplyStruct reply;

if (genieSetup ("/dev/ttyAMA0", 115200) < 0){
    fprintf (stderr, "Proyect cant initialise: %s\n", strerror(errno));
    return 1;
}

    genieWriteObj (GENIE_OBJ_FORM, 0, 0);
    genieWriteObj (GENIE_OBJ_FORM, 1, 0);

    (void)pthread_create (&threadSample, NULL, handleAngularMeter, NULL);
    (void)pthread_create (&threadTiempo, NULL, handleTime, NULL);

    for (;;)
    {
        while(genieReplyAvail())
        {
            genieGetReply(&reply);
            handleGenieEvent(&reply);
        }
        delay(10);
    }
  return 0;
}
