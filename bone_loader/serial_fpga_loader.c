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
#include "gpio.h"

#define CONFIG_CYCLES 1

#define CFG_DONE        48      // GPIO_48
#define CFG_PROG        30      // GPIO_30
#define CFG_INIT        31      // GPIO_31
#define CFG_DELAY       1

#define SPI_MAX_LENGTH 4096
static int  spi_fd;
static int  cfg_done_fd;
static int  cfg_prog_fd;
static int  cfg_init_fd;

static const char * device = "/dev/spidev1.0";
static unsigned int mode = 0 ;
static unsigned int bits = 8 ;
static unsigned long speed = 16000000UL ;
// static unsigned long speed = 1000000UL ;

unsigned char configBits[1024*1024*4], configDummy[1024*1024*4];


void initGPIOs();
void closeGPIOs();
void clearProgramm();
void setProgramm();
char checkDone();
char checkInit();

void __delay_cycles(unsigned long cycles);

char serialConfig(unsigned char * buffer, unsigned int length);

void serialConfigWriteByte(unsigned char val);


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
    int ret ;
    spi_fd = open(device, O_RDWR);
    if (spi_fd < 0){
        printf("can't open device\n");
        return -1 ;
    }

    ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1){
        printf("can't set spi mode \n");
        return -1 ;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1){
        printf("can't get spi mode \n ");
        return -1 ;
    }

    /*
     * bits per word
     */
    ret = ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1){
        printf("can't set bits per word \n");
        return -1 ;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1){
        printf("can't get bits per word \n");
        return -1 ;
    }

    /*
     * max speed hz
     */
    ret = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1){
        printf("can't set max speed hz \n");
        return -1 ;
    }

    ret = ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1){
        printf("can't get max speed hz \n");
        return -1 ;
    }


    return 1;
}

void initGPIOs()
{
    // export and configure the pin for our usage
    gpio_export(CFG_PROG);
    gpio_set_dir(CFG_PROG, GPIO_DIR_OUTPUT);
    cfg_prog_fd = gpio_fd_open(CFG_PROG);
    gpio_export(CFG_INIT);
    gpio_set_dir(CFG_INIT, GPIO_DIR_INPUT);
    cfg_init_fd = gpio_fd_open(CFG_INIT);
    gpio_export(CFG_DONE);
    gpio_set_dir(CFG_DONE, GPIO_DIR_INPUT);
    cfg_done_fd = gpio_fd_open(CFG_DONE);
    printf("TODO: error handeling for initGPIOs()\n");
    return;
}

void closeGPIOs(){
    gpio_fd_close(cfg_prog_fd);
    gpio_fd_close(cfg_init_fd);
    gpio_fd_close(cfg_done_fd);
}


void resetFPGA(){
    // set CFG_PROG(0) to switch FPGA to RECONFIG mode
    gpio_set_value_fd(cfg_prog_fd, 0);
    return;
}

char serialConfig(unsigned char * buffer, unsigned int length){
    unsigned int timer = 0;
    unsigned int write_length, write_index ;
    
    gpio_set_value_fd(cfg_prog_fd, 1);
    __delay_cycles(10 * CFG_DELAY);
    gpio_set_value_fd(cfg_prog_fd, 0);
    __delay_cycles(5 * CFG_DELAY);

    while (gpio_get_value_fd(cfg_init_fd) > 0 && timer < 900) {
        timer++; // waiting for init pin to go down
    }
    if (timer >= 900) {
        printf("ERROR: FPGA did not answer to prog request, init pin not going low \n");
        gpio_set_value_fd(cfg_prog_fd, 1);
        return -1;
    }

    timer = 0;
    __delay_cycles(5 * CFG_DELAY);
    gpio_set_value_fd(cfg_prog_fd, 1);
    while (gpio_get_value_fd(cfg_init_fd) == 0 && timer < 256) { // need to find a better way ...
        timer++; // waiting for init pin to go up
    }
    if (timer >= 256) {
        printf("ERROR: FPGA did not answer to prog request, init pin not going high \n");
        return -1;
    }

    timer = 0;
    write_length = min(length, SPI_MAX_LENGTH);
    write_index = 0 ;
    while(length > 0){
        if(write(spi_fd, &buffer[write_index], write_length) < write_length){
            printf("spi write error \n");
        }
        write_index += write_length ;
        length -= write_length ;
        write_length = min(length, SPI_MAX_LENGTH);
    }

    if (gpio_get_value_fd(cfg_done_fd) == 0) {
        printf("ERROR: FPGA prog failed, done pin not going high \n");
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

    closeGPIOs();
    fclose(fr);
    close(spi_fd);
    return 1;
}
