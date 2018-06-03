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
#include <libgen.h>

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
#include "minIni.h"

struct timespec key_press, key_release, keypress_time;
extern GPIOPIN_IN gpio_input;
extern gpio_handle_t user_key;

static volatile int bpress;

char IniFile[64];

struct config {
	int Delay;

	struct {
		bool Enabled;
		char APIKey[48];
	} M2X;

	struct {
		bool Enabled;
		char Id[64], 
		     Secret[64], 
		     Org[64],
		     Proj[64], 
		     Comp[16], 
		     Class[16];		
	} DataFlow;

	struct {
		bool Enabled;
		char Host[64],
		     Port[10];		
	} UDP;

	struct {
		float Latitude,
		      Longitude;		
	} GPS;
} Config;


bool load_config() {
    ssize_t len = readlink("/proc/self/exe", IniFile, sizeof(IniFile));
    if (len > 0) {
        IniFile[len] = 0;
        strcat(IniFile, ".ini" );
        printf("Ini file %s\n", IniFile);
    } else {
        printf("Cannot determine .ini file name\n");
        return false;
    };
    
    Config.Delay = ini_getl("", "Delay", 30, IniFile);
    Config.M2X.Enabled = ini_getbool("M2X", "Enable", 0, IniFile);        
    int c = ini_gets("M2X", "APIKey", "", Config.M2X.APIKey, sizeof(Config.M2X.APIKey), IniFile);
    if (Config.M2X.Enabled && !c) {
        printf("Cannot read M2X API key from .ini file\n");
    	return false;    
    };

    Config.DataFlow.Enabled = ini_getbool("DataFlow", "Enable", 0, IniFile);
    ini_gets("DataFlow","ClientID","", Config.DataFlow.Id, sizeof(Config.DataFlow.Id), IniFile);
    ini_gets("DataFlow","ClientSecret","", Config.DataFlow.Secret, sizeof(Config.DataFlow.Secret), IniFile);
    ini_gets("DataFlow","Organization","", Config.DataFlow.Org, sizeof(Config.DataFlow.Org), IniFile);
    ini_gets("DataFlow","Project","", Config.DataFlow.Proj, sizeof(Config.DataFlow.Proj), IniFile);
    ini_gets("DataFlow","Component","", Config.DataFlow.Comp, sizeof(Config.DataFlow.Comp), IniFile);
    ini_gets("DataFlow","Class","", Config.DataFlow.Class, sizeof(Config.DataFlow.Class), IniFile); 

    Config.UDP.Enabled = ini_getbool("UDP", "Enable", 0, IniFile);
    ini_gets("UDP","Host","", Config.UDP.Host, sizeof(Config.UDP.Host), IniFile);
    ini_gets("UDP","Port","", Config.UDP.Port, sizeof(Config.UDP.Port), IniFile);

    Config.GPS.Latitude = ini_getf("GPS", "Latitude", 0, IniFile);
    Config.GPS.Longitude = ini_getf("GPS", "Longitude", 1000, IniFile);

    return true;
}


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
    float        lat, lng;
    json_keyval    om[20];
    struct timeval start, end;  //measure duration of flow calls...
    bool gps_fix = false;    

    if (!load_config()) {
        printf("Failed to load configuration.\n");
        return 0;
    }
    delay_time = Config.Delay;
    doM2X = Config.M2X.Enabled;
    lat = Config.GPS.Latitude;
    lng = Config.GPS.Longitude;

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
    mySystem.imei=getIMEI(om, sizeof(om));
    printf("IMEI: %s\n",mySystem.imei.c_str());

    mySystem.appsVer=getAppsVersion(om, sizeof(om));
    mySystem.firmVer=getFirmwareVersion(om, sizeof(om));
    printf("Firmware: %s, app %s\n", mySystem.firmVer.c_str(), mySystem.appsVer.c_str());
    
    enableGPS();    

    if (doM2X) {
        printf("-Validating API Key and Device ID...\n");
        m2x_device_info(Config.M2X.APIKey, device_id,  resp);
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
                strncpy(Config.M2X.APIKey,strptr+7,i+1);
            };
            strptr = strstr(resp,"\"id\":\"");
            if (strptr) {
                i=strcspn(strptr+7,"\"");
                strncpy(device_id,strptr+6,i+1);
            };
        } else {
            printf("Create a new device.\n");
            if( !strlen(Config.M2X.APIKey) ) {
                printf("ERROR: must provide an API key!\n");
                printf("\n\nExiting Quick Start Application.\n");
                gpio_deinit( &user_key);
                binario_io_close();
                binary_io_init();
                return 0;
                }
            printf("Attempting to create device with id %s\n",  device_id);
            m2x_create_device(Config.M2X.APIKey, device_id, resp);
            i = parse_maljson(resp, om, sizeof(om));
            strcpy(device_id, om[11].value);
            }

        printf("-Creating the data streams...\n");
        m2x_create_stream(device_id, Config.M2X.APIKey, "ADC");
        m2x_create_stream(device_id, Config.M2X.APIKey, "TEMP");
        m2x_create_stream(device_id, Config.M2X.APIKey, "XVALUE");
        m2x_create_stream(device_id, Config.M2X.APIKey, "YVALUE");
        m2x_create_stream(device_id, Config.M2X.APIKey, "ZVALUE");
        m2x_create_stream(device_id, Config.M2X.APIKey, "GPS_STATUS");    
        printf("Using API Key = %s, Device Key = %s\n",Config.M2X.APIKey, device_id);
    } else
        printf("M2X disabled\n");

    if (Config.DataFlow.Enabled) {
        dataflow_get_token(Config.DataFlow.Id, Config.DataFlow.Secret);
        dataflow_create_object(Config.DataFlow.Org, Config.DataFlow.Proj, Config.DataFlow.Class, mySystem.iccid.c_str());
    } else {
        printf("DataFlow disabled\n");
    }

    if (Config.UDP.Enabled) {
        printf("UDP enabled\n");
        udp_send_init(Config.UDP.Host, Config.UDP.Port, mySystem.imei.c_str(), mySystem.firmVer.c_str(), mySystem.appsVer.c_str());
    }

    printf("LED colors will display a different colors after each set of sensor data is sent to M2X.\n");
    printf("\n");
    printf("To exit the Quick Start Application, press the User Button on the Global \n");
    printf("LTE IoT Starter Kit for > 3 seconds.\n\n");
    i=1;
    while( !done ) {
        if( keypress_time.tv_sec > 3 ) {
            done = 1;
            continue;
        };
  
        do_color(current_color="BLUE");
    
        gettimeofday(&start, NULL);
        adc_init(&my_adc);
        adc_read(my_adc, &adc_voltage);
        adc_deinit(&my_adc);
        memset(str_val, 0, sizeof(str_val));
        sprintf(str_val, "%f", adc_voltage);

        printf("%2d. Sending ADC value",i++);
        fflush(stdout);
        m2x_update_stream_value(device_id, Config.M2X.APIKey, "ADC", str_val);		
    
        printf(", TEMP value");
        fflush(stdout);
        sprintf(str_val, "%f", lis2dw12_readTemp12());
        m2x_update_stream_value(device_id, Config.M2X.APIKey, "TEMP", str_val);		

        ptr=lis2dw12_m2x();

        printf(", XYZ values");
        fflush(stdout);
        m2x_update_stream_value(device_id, Config.M2X.APIKey, "XVALUE", ptr[0]);		
        m2x_update_stream_value(device_id, Config.M2X.APIKey, "YVALUE", ptr[1]);		
        m2x_update_stream_value(device_id, Config.M2X.APIKey, "ZVALUE", ptr[2]);		

        k=getGPSlocation(om,sizeof(om));
        for(int idx=1; idx<k; idx++ ) {
            if( !strcmp(om[idx].key,"loc_status") ) {
                m2x_update_stream_value(device_id, Config.M2X.APIKey, "GPS_STATUS", atoi(om[idx].value)? "1" : "0");
                gps_fix = om[idx].value[0] != '0';
            } else if( !strcmp(om[idx].key,"latitude") ) {
                lat = atof(om[idx].value);
            } else if( !strcmp(om[idx].key,"longitude") ) {
                lng = atof(om[idx].value);
            };
        };
/*        
        gps_fix = true;
        lat = 47.669;
        lng = -122.295;
*/
        if (gps_fix && doM2X) {
            printf(", location");            
            m2x_update_location(device_id, Config.M2X.APIKey,lat,lng);
        };

        printf(", DataFlow ...");
        fflush(stdout);
        get_wwan_status(om, sizeof(om));        
        if (Config.DataFlow.Enabled)
            dataflow_ingest_data(Config.DataFlow.Org, Config.DataFlow.Proj, Config.DataFlow.Comp, 
                                Config.DataFlow.Class, mySystem.iccid.c_str(), 
                                adc_voltage, lis2dw12_readTemp12(), ptr, om[4].value,
                                gps_fix, lat, lng);

        if (Config.UDP.Enabled)
            udp_send(Config.UDP.Host, Config.UDP.Port, mySystem.imei.c_str(), 
                     adc_voltage, lis2dw12_readTemp12(), ptr, 
                     gps_fix, lat, lng, 
                     om[4].value);

        printf(" all Values sent.");
        fflush(stdout);

        do_color(current_color = gps_fix ? "GREEN" : "MAGENTA");

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


