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

    @file          http.h
    @version       1.0
    @date          Sept 2017

======================================================================== */


#ifndef __HTTP_H__
#define __HTTP_H__

#ifdef __cplusplus
extern "C" {
#endif

extern void m2x_device_info ( const char *api_key_ptr, char *iccid_ptr, char *ret_buffer);
extern void m2x_list_devices ( const char *api_key_ptr, char *buff );
extern int m2x_create_device ( const char *api_key_ptr, const char *device_name_ptr, char *buff );
extern int m2x_create_stream ( const char *device_id_ptr, const char *api_key_ptr, const char *stream_name_ptr );
extern int m2x_update_stream_value ( const char *device_id_ptr, const char *api_key_ptr, 
                                 const char *stream_name_ptr, const char *stream_value_ptr);
extern int m2x_update_location ( const char *device_id_ptr, const char *api_key_ptr, 
                                 float lat, float lng);                                     
extern char *flow_get ( const char *flow_base_url, const char *flow_input_name, 
                 const char *flow_device_name, const char *flow_server, const char *get_cmd, char *response, int resp_size);
extern int flow_put( const char *flow_base_url, const char *flow_input_name, 
                 const char *flow_device_name, const char *flow_server, const char *post_cmd );

extern int dataflow_get_token( const char *id, const char *secret);
extern int dataflow_ingest_data( const char *org, const char *project, const char *component, 
                                 const char *cls, const char *id, 
                                 float light, float temp,  char **accel, const char *sig_strength,
                                 int gps, float lat, float lng);
extern int dataflow_create_object( const char *org, const char *project, const char *classid, const char *id);

extern int udp_send(const char *host, const char *port, const char* imei, 
                    float light, float temp, char **accel, 
                    int gps, float lat, float lng, 
                    const char* sig_strength);

extern int udp_send_init(const char *host, const char *port, const char* imei, 
                    const char *firmware, const char *app);                    

int m2x_update_color_value ( const char *device_id_ptr, const char *api_key_ptr, const char *stream_name_ptr, const char *stream_value_ptr);

#ifdef __cplusplus
}
#endif

#include <sys/types.h>
#include <stdint.h>
#include <nettle/nettle-stdint.h>
#include <nettle/nettle-stdint.h>
#include <hwlib/hwlib.h>

#endif // __HTTP_H__
