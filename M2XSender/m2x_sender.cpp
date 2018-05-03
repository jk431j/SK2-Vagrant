/* =====================================================================
   Copyright © 2016, Avnet (R)

   www.avnet.com 
 
   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, 
   software distributed under the License is distributed on an 
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, 
   either express or implied. See the License for the specific 
   language governing permissions and limitations under the License.

    @file          qsapp.cpp
    @version       1.0
    @date          Sept 2017

======================================================================== */


#include <unistd.h>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <hwlib/hwlib.h>
void wwan_io(int);
char  **lis2dw12_m2x(void);

#ifdef __cplusplus
}
#endif

#include "iot_monitor.h"
#include "microrl_config.h"
#include "microrl.h"
#include "HTS221.hpp"
#include "lis2dw12.h"
#include "binio.h"
#include "mytimer.h"
#include "http.h"
#include "m2x.h"

#include "mal.hpp"

struct timespec key_press, key_release, keypress_time;
extern GPIOPIN_IN gpio_input;
extern gpio_handle_t user_key;

static volatile int bpress;

const char *current_color=NULL;
static const char *colors[] = {
    "BLUE",
    "GREEN",
    "BLUE",
    "MAGENTA",
    "TURQUOISE",
    "RED",
    "WHITE",
    "YELLOW",
    "CYAN"
    };

#define red_led		gpios[0].hndl
#define green_led	gpios[1].hndl
#define blue_led	gpios[2].hndl

#define RED_LED        1  //0001 RED
#define GREEN_LED      2  //0010 GREEN
#define YELLOW_LED     3  //0011 RED + GREEN
#define BLUE_LED       4  //0100
#define MAGENTA_LED    5  //0101 BLUE + RED
#define CYAN_LED       6  //0110 GREEN+ BLUE 
#define WHITE_LED      7  //0111 GREEN+BLUE+RED

#define WAIT_FOR_BPRESS(x) {while( !x ); while( x );}

void do_color( const char *color )
{
    int val=0;

    if( !strcmp(color, "BLUE") )
        val=BLUE_LED;
    else if( !strcmp(color, "GREEN") )
        val=GREEN_LED;
    else if( !strcmp(color, "BLUE") )
        val=BLUE_LED;
    else if( !strcmp(color, "MAGENTA") )
        val=MAGENTA_LED;
    else if( !strcmp(color, "TURQUOISE") )
        val=CYAN_LED;
    else if( !strcmp(color, "RED") )
        val=RED_LED;
    else if( !strcmp(color, "WHITE") )
        val=WHITE_LED;
    else if( !strcmp(color, "YELLOW") )
        val=YELLOW_LED;
    else if( !strcmp(color, "CYAN") )
        val=CYAN_LED;
    else
        val=0;

    gpio_write( red_led, (val&RED_LED)?GPIO_LEVEL_HIGH:GPIO_LEVEL_LOW );
    gpio_write( green_led, (val&GREEN_LED)?GPIO_LEVEL_HIGH:GPIO_LEVEL_LOW );
    gpio_write( blue_led, (val&BLUE_LED)?GPIO_LEVEL_HIGH:GPIO_LEVEL_LOW );
}

int qsa_irq_callback(gpio_pin_t pin_name, gpio_irq_trig_t direction)
{
    if (pin_name != GPIO_PIN_98) 
        return 0;

    if( bpress = !bpress ) {
        do_color("WHITE");
        clock_gettime(CLOCK_MONOTONIC, &key_press);
        }
    else{
        do_color(current_color);
        clock_gettime(CLOCK_MONOTONIC, &key_release);
        if ((key_release.tv_nsec-key_press.tv_nsec)<0) 
            keypress_time.tv_sec = key_release.tv_sec-key_press.tv_sec-1;
        else 
            keypress_time.tv_sec = key_release.tv_sec-key_press.tv_sec;
        }
    return 0;
}

//
// two ways to leave this program are either with a ^C (if the IOT Application is running) or with an "EXIT" command
// from the monitor.  In both cases the following function is called...

void app_exit(void)
{
//    clean-up, free any malloc'd memory, exit
    binario_io_close();
    printf("-exiting...\n");

    exit(EXIT_SUCCESS);
}

int main(int argc, const char * const * argv )
{
    void  wwan_io(int);
    void  do_lis2dw2m2x(void);
    float lis2dw12_readTemp12(void);

    adc_handle_t my_adc=(adc_handle_t)NULL;
    int          start_data_service(void);
    int          done=0, k=0, i, c;
    int          dly, delay_time;
    char         resp[1024], qsa_url[100];
    char         color[10];
    char         **ptr, **lis2dw12_m2x(void);
    char         str_val[16];
    double       elapse=0;
    float        adc_voltage;

    json_keyval    om[20];
    struct timeval start, end;  //measure duration of flow calls...


    if( argc < 3 ) {
        printf("Required parameters missing: delay api_key\n");
	return 0;
    } else {
        delay_time = atoi(argv[1]);
	if (delay_time < 5)
	   delay_time = 5;

	strcpy(api_key, argv[2]);
    };


    c=start_data_service();
    while ( c < 0 ) {
        printf("WAIT: starting WNC Data Module (%d)\n",c);
        sleep(10);
        c=start_data_service();
        }

    monitor_wwan();
    binary_io_init();

    c=lis2dw12_initialize();
    c=lis2dw12_getDeviceID();

    do_color(current_color="RED");
    mySystem.iccid=getICCID(om, sizeof(om));
    strcpy(device_id,  mySystem.iccid.c_str());
    printf("Using ICCID %s as M2X device serial\n", device_id);
    
    gpio_deinit( &gpio_input.hndl);
    bpress = 0;
    gpio_init( GPIO_PIN_98,  &user_key );  //SW3
    gpio_dir(user_key, GPIO_DIR_INPUT);
    gpio_irq_request(user_key, GPIO_IRQ_TRIG_BOTH, qsa_irq_callback);

    get_wwan_status(om, sizeof(om));
    while (strcmp(om[7].value,"1")) {
        sleep(10);
        get_wwan_status(om, sizeof(om));
    };

    printf("-Validating API Key and Device ID...\n");
    m2x_device_info(api_key, device_id,  resp);
    if (strcasestr(resp,"invalid API key")) {
	printf("Please specify valid API key. This is what I got from M2X:\n%s\n", resp);
        return 0;
    }

    char *strptr = strstr(resp,"\"name\":\"Global Starter Kit\",");
    if (strptr) {
        printf("device already present.\n");
        strptr = strstr(resp,"\"key\":\"");
        if (strptr) {
            i=strcspn(strptr+8,"\"");
            strncpy(api_key,strptr+7,i+1);
        };
        strptr = strstr(resp,"\"id\":\"");
        if (strptr) {
            i=strcspn(strptr+7,"\"");
            strncpy(device_id,strptr+6,i+1);
        };
    } else {
        printf("Create a new device.\n");
        if( !strlen(api_key) ) {
            printf("ERROR: must provide an API key!\n");
            printf("\n\nExiting Quick Start Application.\n");
            gpio_deinit( &user_key);
            binario_io_close();
            binary_io_init();
            return 0;
            }
        printf("Attempting to create device with id %s\n",  device_id);
        m2x_create_device(api_key, device_id, resp);
        i = parse_maljson(resp, om, sizeof(om));
        strcpy(device_id, om[11].value);
        }

    printf("-Creating the data streams...\n");
    m2x_create_stream(device_id, api_key, "ADC");
    m2x_create_stream(device_id, api_key, "TEMP");
    m2x_create_stream(device_id, api_key, "XVALUE");
    m2x_create_stream(device_id, api_key, "YVALUE");
    m2x_create_stream(device_id, api_key, "ZVALUE");

    sprintf(qsa_url, "https://api-m2x.att.com/devices/%s", device_id);
    printf("Using API Key = %s, Device Key = %s\n",api_key, device_id);
    printf("LED colors will display a different colors after each set of sensor data is sent to M2X.\n");
    printf("\n");
    printf("To exit the Quick Start Applicatioin, press the User Button on the Global \n");
    printf("LTE IoT Starter Kit for > 3 seconds.\n\n");
    i=1;
    while( !done ) {
        if( keypress_time.tv_sec > 3 ) {
            done = 1;
            continue;
            }
        do_color(current_color="BLUE");
    
        gettimeofday(&start, NULL);
        adc_init(&my_adc);
        adc_read(my_adc, &adc_voltage);
        adc_deinit(&my_adc);
        memset(str_val, 0, sizeof(str_val));
        sprintf(str_val, "%f", adc_voltage);

        printf("%2d. Sending ADC value",i++);
        fflush(stdout);
        m2x_update_stream_value(device_id, api_key, "ADC", str_val);		
    
        printf(", TEMP value");
        fflush(stdout);
        sprintf(str_val, "%f", lis2dw12_readTemp12());
        m2x_update_stream_value(device_id, api_key, "TEMP", str_val);		

        ptr=lis2dw12_m2x();

        printf(", XYZ values...");
        fflush(stdout);
        m2x_update_stream_value(device_id, api_key, "XVALUE", ptr[0]);		
        m2x_update_stream_value(device_id, api_key, "YVALUE", ptr[1]);		
        m2x_update_stream_value(device_id, api_key, "ZVALUE", ptr[2]);		

        printf("All Values sent.");
        fflush(stdout);

        do_color(current_color="GREEN");

        gettimeofday(&end, NULL);
        elapse = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec/1000 - start.tv_usec/1000);
        dly = ((delay_time*1000)-round(elapse))/1000;
        if( dly > 0) {
            printf(" (delay %d seconds)\n",dly);
            sleep(dly);
            }
        else
            printf("\n");
        }
    printf("\n\nExiting Quick Start Application.\n");

    do_color("OFF");
    gpio_deinit( &user_key);
    app_exit();
}


void do_emissions_test(void);  //this test is for performing CE/FCC emissions testing
int emission_test = 0;

//
// This function is used to output data to the terminal as it is typed
//
static void my_putchar(const char *c)
{
    printf("%s",c);
}

//
// Because can be used by both C++ and C files, make sure to define it as a C
// function. all it does is output a BS to the terminal. Its intended usage is
// when asyncronus message text is output to the user.  This will help restore
// the prompt for the user to avoid confusion.
//
#ifdef __cplusplus
extern "C" {
#endif

void doNewLine(void)
{
    microrl_insert_char(prl, ' ');
    microrl_insert_char(prl, KEY_BS);
}

#ifdef __cplusplus
}
#endif


