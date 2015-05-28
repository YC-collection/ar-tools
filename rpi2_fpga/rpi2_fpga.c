#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h> 
#include <math.h>
#include <pigpio.h>

#define CONFIG_CYCLES 1

#define CFG_DONE        13      // GPIO_13
#define CFG_PROG         5      // GPIO_05
#define CFG_INIT         6      // GPIO_06
#define CFG_DELAY        1

#define SPI_MAX_LENGTH 4096
// #define SPI_MAX_LENGTH 64
static int  spi_fd;

static unsigned int mode = 0 ;
// failed for RPi2: static unsigned long speed = 16000000UL ;
// static unsigned long speed = 15000000UL ;
// static unsigned long speed =  1000000UL ;
// static unsigned long speed =   100000UL ;
static unsigned long speed =  9500000UL ;

char configBits[1024*1024*4], configDummy[1024*1024*4];

void initGPIOs();
void closeGPIOs();
void clearProgramm();
void setProgramm();
char checkDone();
char checkInit();

void __delay_cycles(unsigned long cycles);

int serialConfig(char * buffer, unsigned int length);

void printHelp(){
    printf("Usage : logi_loader -[r|h] <bitfile> \n");
    printf("-r	will put the FPGA in reset state (lower power consumption)\n");
    printf("-h      will print the help \n");
}

void __delay_cycles(unsigned long cycles){
    while(cycles != 0){
        cycles -- ;	
    }
}


static inline unsigned int min(unsigned int a, unsigned int b){
    if(a < b) return a ;
    return b ;
}

int init_spi(void){
    uint32_t spiFlags;
    // refer to http://abyz.co.uk/rpi/pigpio/cif.html#spiOpen
    //             bbbbbb      R           T           nnnn        W          A          ux         px        mm
    // spiFlags = (0 << 16) | (0 << 15) | (0 << 14) | (0 << 10) | (0 << 9) | (0 << 8) | (0 << 5) | (0 << 2) | mode;
    spiFlags = mode;
    spi_fd = spiOpen(0, speed, spiFlags);
    printf("spi_fd(%d)\n", spi_fd);

    return spi_fd;
}

void initGPIOs()
{
    if (gpioInitialise() < 0)
    {
        fprintf(stderr, "pigpio initialisation failed\n");
        return;
    }
    // export and configure the pin for our usage
    gpioSetMode(CFG_PROG, PI_OUTPUT);
    gpioSetMode(CFG_INIT, PI_INPUT);
    gpioSetMode(CFG_DONE, PI_INPUT);
    return;
}

void closeGPIOs(){
    /* stop DMA, release resources */
    gpioTerminate();
}

void resetFPGA(){
    // set CFG_PROG(0) to switch FPGA to RECONFIG mode
    gpioWrite(CFG_PROG, 0);
    return;
}

int serialConfig(char * buffer, unsigned int length){
    unsigned int timer = 0;
    unsigned int write_length, write_index ;
    
    gpioWrite(CFG_PROG, 1);
    __delay_cycles(10 * CFG_DELAY);
    gpioWrite(CFG_PROG, 0);
    __delay_cycles(5 * CFG_DELAY);

    while (gpioRead(CFG_INIT) > 0 && timer < 900) {
        printf("timer(%d) CFG_INIT(%d)\n", timer, gpioRead(CFG_INIT));
        timer++; // waiting for init pin to go down
    }
    printf("timer(%d) CFG_INIT(%d)\n", timer, gpioRead(CFG_INIT));
    if (timer >= 900) {
        printf("ERROR: FPGA did not answer to prog request, init pin not going low \n");
        gpioWrite(CFG_PROG, 1);
        return -1;
    }

    timer = 0;
    __delay_cycles(5 * CFG_DELAY);
    gpioWrite(CFG_PROG, 1);
    while (gpioRead(CFG_INIT) == 0 && timer < 256) { // need to find a better way ...
        printf("timer(%d) CFG_INIT(%d)\n", timer, gpioRead(CFG_INIT));
        timer++; // waiting for init pin to go up
    }
    printf("timer(%d) CFG_INIT(%d)\n", timer, gpioRead(CFG_INIT));
    if (timer >= 256) {
        printf("ERROR: FPGA did not answer to prog request, init pin not going high \n");
        return -1;
    }

    timer = 0;
    write_length = min(length, SPI_MAX_LENGTH);
    write_index = 0 ;
    while(length > 0){
        // if(write(spi_fd, &buffer[write_index], write_length) < write_length){
        //     printf("spi write error \n");
        // }
        if(spiWrite(spi_fd, &buffer[write_index], write_length) < write_length)
        {
            printf("spi write error \n");
        }
        printf ("length(%d) write_length(%d)\n", length, write_length);
        write_index += write_length ;
        length -= write_length ;
        write_length = min(length, SPI_MAX_LENGTH);
    }
    
    if (gpioRead(CFG_DONE) == 0) {
        printf("ERROR: FPGA prog failed, done pin not going high \n");
        printf("\ttimer(%d) CFG_DONE(%d)\n", timer, gpioRead(CFG_DONE));
        return -1;
    }

    return length;
}


int main(int argc, char ** argv){
    unsigned int i ;
    FILE * fr;
    unsigned int size = 0 ;	

    if (argc == 1) {
        printHelp();
        exit(EXIT_FAILURE);
    }

    //parse programm args
    for(i = 1 ; i < argc ; ){
        if(argv[i][0] == '-'){
            switch(argv[i][1]){
                case '\0': 
                    i ++ ;
                    break ;
                case 'r' :
                    resetFPGA(); 
                    closeGPIOs();
                    return 1 ;
                    break ;
                case 'h' :
                    printHelp();
                    return 1 ;
                    break;
                default :
                    printHelp();
                    return 1 ;
                    break ;
            }
        }else{
            //last argument is file to load
            break ;
        }
    }

    
    initGPIOs();

    if(init_spi() < 0){
        printf("cannot open spi bus \n");
        return -1 ;
    }
    fr = fopen (argv[i], "rb");  /* open the file for reading bytes*/
    if(fr < 0){
        printf("cannot open file %s \n", argv[1]);
        return -1 ;	
    }
    memset((void *) configBits, (int) 0, (size_t) 1024*1024);
    size = fread(configBits, 1, 1024*1024, fr);
    printf("bit file size : %d \n", size);

    //8*5 clock cycle more at the end of config
    if(serialConfig(configBits, size+5) < 0){
        printf("config error \n");
        exit(0);	
    }else{
        printf("config success ! \n");	
    }

    if (spiClose(spi_fd) != 0)
    {
        printf("config error \n");
    }
    closeGPIOs();
    fclose(fr);
    return 1;
}
