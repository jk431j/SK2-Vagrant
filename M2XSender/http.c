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

    @file          http.c
    @version       1.0
    @date          Sept 2017

======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "iot_monitor.h"
#include "http.h"

void wnctest(void);

#define RXMSG_SIZE	250

typedef struct _http_info_s {
	CURL *curl;
	const char *url;
	struct curl_slist *header;
	struct curl_slist *data_field;
	struct curl_slist *last_data;
	size_t total_len;
        char *rx_msg;
        size_t rx_msglen; 
} http_info_t;

static int rsize;

static char auth_token[128] = "beefbeefbeef";
static char df_id[64];
static char df_secret[64];

static int http_deinit(http_info_t *req)
{
    if(!req)
        return -1;
    if (req->header != NULL)  { 
        curl_slist_free_all(req->header); 
        req->header = NULL; 
        } 
    if (req->data_field != NULL) { 
        curl_slist_free_all(req->data_field); 
        req->data_field = NULL; 
        } 
    curl_easy_cleanup(req->curl); 
    free(req->rx_msg);
    req->rx_msglen=0;
    return 0;
}

static int http_init(http_info_t *http_req, const int is_https)
{

    http_deinit(http_req);

    http_req->curl = curl_easy_init();

    if (is_https)
    	curl_easy_setopt(http_req->curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_easy_setopt(http_req->curl, CURLOPT_CRLF, 1L);

    if( dbg_flag & DBG_CURL )
        curl_easy_setopt(http_req->curl, CURLOPT_VERBOSE, 1L);
    http_req->rx_msg   = malloc(RXMSG_SIZE);
    http_req->rx_msglen= 0;
    
    return 0;
}

static struct curl_slist *http_add_field(struct curl_slist *ptr, const char *str)
{
	ptr = curl_slist_append(ptr, str);
	return ptr;
}

static int http_field_len(struct curl_slist *ptr)
{
    struct curl_slist *sl; 
    int len=0;

    for(sl=ptr; sl!=NULL ; sl = sl->next) 
        len += strlen(sl->data); 
    return len;
}

//
// This is setup so userp points to the callers http_info_t structure, this contains the
// pointer to the data buffer and buffer length.
//
// buff_len always points to the end of the current buffer data
//

size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    http_info_t *http_req = (http_info_t *)userp;
    size_t chunk_size = (nmemb*size)+1;       //+1 to hold the NULL at the end
    size_t inptr = http_req->rx_msglen;
        char *rx_msg;

    if( (inptr + chunk_size) > http_req->rx_msglen ) 
        http_req->rx_msg = realloc(http_req->rx_msg, inptr+chunk_size);

    memcpy(&(http_req->rx_msg[inptr]), contents, chunk_size);
    http_req->rx_msg[chunk_size] = 0x00;
//printf("JIM4: %s\n",http_req->rx_msg);

    return (http_req->rx_msglen = (inptr+chunk_size)-1);  //don't include trailing NULL in length
}


size_t http_callback(struct curl_slist *req, size_t total_len, char *buffer, size_t size, size_t nitems)
{
    struct curl_slist *sl;
    size_t max_length = size * nitems;
    size_t to_copy = 0;

    if (total_len <= 0) {
        req = NULL;
        return 0;
        }

    for (sl=req ; sl!=NULL ; sl = sl->next) {
        size_t len = strlen(sl->data);
        if ((to_copy + len) > max_length) {
            req = sl;
            break;
            }
        memcpy(&buffer[to_copy], sl->data, len);
        if( dbg_flag & DBG_M2X ) {
            printf("\nSENDING DATA: ");
            for(int i=0; i<len; i++) printf("%c",sl->data[i]); printf("\n");
            }
        to_copy += len;
        }
    total_len -= to_copy;
    return to_copy;
}


size_t http_upload_read_callback(char *buffer, size_t size, size_t nitems, void *instream)
{
    http_info_t *http_req = (http_info_t *)instream;
    return http_callback(http_req->last_data, http_req->total_len, buffer, size, nitems);
}


size_t http_post_read_callback(char *buffer, size_t size, size_t nitems, void *instream)
{
    http_info_t *http_req = (http_info_t *)instream;
    return http_callback(http_req->last_data, http_req->total_len, buffer, size, nitems);
}


static int http_post(http_info_t *http_req, const char *url)
{
    CURLcode res;

    if ((http_req == NULL) || (url == NULL)) 
    	return -1;

    curl_easy_setopt(http_req->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(http_req->curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(http_req->curl, CURLOPT_READDATA, (void *)http_req);
    curl_easy_setopt(http_req->curl, CURLOPT_READFUNCTION, http_post_read_callback);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEDATA, (void *)http_req);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(http_req->curl, CURLOPT_URL, url);

    http_req->total_len = http_field_len(http_req->data_field);
    if (http_req->total_len >= 0)
	curl_easy_setopt(http_req->curl, CURLOPT_POSTFIELDSIZE, http_req->total_len);
    http_req->last_data = http_req->data_field;

    if (http_req->header != NULL)
	curl_easy_setopt(http_req->curl, CURLOPT_HTTPHEADER, http_req->header);

    return ((res = curl_easy_perform(http_req->curl)) != CURLE_OK)? -1 : 0; 
}

static int http_put(http_info_t *http_req, const char *url)
{
    CURLcode res;

    if ((http_req == NULL) || (url == NULL)) {
        return -1;
        }

    if (http_req->curl == NULL) 
        return -1;

    curl_easy_setopt(http_req->curl, CURLOPT_PUT, 1L);
    curl_easy_setopt(http_req->curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(http_req->curl, CURLOPT_READDATA, (void *)http_req);
    curl_easy_setopt(http_req->curl, CURLOPT_READFUNCTION, http_upload_read_callback);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEDATA, (void *)http_req);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(http_req->curl, CURLOPT_URL, url);

    http_req->total_len = http_field_len(http_req->data_field);
    if (http_req->total_len >= 0)
        curl_easy_setopt(http_req->curl, CURLOPT_INFILESIZE, http_req->total_len);
    http_req->last_data = http_req->data_field;

    if (http_req->header != NULL)
      	curl_easy_setopt(http_req->curl, CURLOPT_HTTPHEADER, http_req->header);

    return ((res = curl_easy_perform(http_req->curl)) != CURLE_OK) ? -1 : 0;
}

//
// This function creates a M2X Stream if it doesn't currently exist.  
//
int m2x_create_stream ( const char *device_id_ptr, const char *api_key_ptr, const char *stream_name_ptr )
{
    int ret =0;
    if( doM2X ) {
	http_info_t put_req;
	char tmp_buff[64];
	char tmp_buff2[64];
	char url[256];

	memset(&put_req, 0, sizeof(http_info_t));
	memset(tmp_buff, 0, sizeof(tmp_buff));
	memset(tmp_buff2, 0, sizeof(tmp_buff2));
	memset(url, 0, sizeof(url));

	http_init(&put_req, 0);

	sprintf(url, "http://api-m2x.att.com/v2/devices/%s/streams/%s", device_id_ptr, stream_name_ptr );
	sprintf(tmp_buff, "X-M2X-KEY:%s", api_key_ptr);

	put_req.header = http_add_field(put_req.header, tmp_buff);
	put_req.header = http_add_field(put_req.header, "Content-Type: application/json");

        if( dbg_flag & DBG_M2X ) {
            printf("\n\n\ncreate_stream\n");
            printf("JIM: %s\n", url);
            printf("JIM: %s\n",tmp_buff);
            }

        ret=http_put(&put_req, url);
        if( put_req.rx_msglen >0) {
            if( strcmp(put_req.rx_msg,"{\"status\":\"accepted\"}") &&  dbg_flag & DBG_M2X ) 
                printf("\nUNEXPECTED REPLY WAS: %s\n",put_req.rx_msg);
            }
	http_deinit(&put_req);
        }
    return ret;
}

//
// This function is called to update a stream value.
//
int m2x_update_stream_value ( const char *device_id_ptr, const char *api_key_ptr, const char *stream_name_ptr, const char *stream_value_ptr)
{
    int ret = 0;

    if( doM2X ) {
        http_info_t post_req;
        char tmp_buff1[256], tmp_buff2[256], tmp_buff3[64];;
        char url[256];
        time_t t = time(NULL);;
        struct tm *tm;

        memset(&post_req, 0, sizeof(http_info_t));
        memset(tmp_buff1, 0, sizeof(tmp_buff1));
        memset(tmp_buff2, 0, sizeof(tmp_buff2));
        memset(url, 0, sizeof(url));

        tm = localtime(&t);
        http_init(&post_req, 0);

        sprintf(url, "https://api-m2x.att.com/v2/devices/%s/streams/%s/values", device_id_ptr, stream_name_ptr);
        sprintf(tmp_buff1, "X-M2X-KEY: %s", api_key_ptr);
        sprintf(tmp_buff3, "%4d-%02d-%02dT%02d:%02d:%02dZ", (tm->tm_year+1900), (tm->tm_mon+1), tm->tm_mday,
                                                            tm->tm_hour, tm->tm_min, tm->tm_sec);
        sprintf(tmp_buff2, "{ \"values\": [ { \"timestamp\": \"%s\", \"value\": %s } ] }", tmp_buff3, stream_value_ptr);

        post_req.header = http_add_field(post_req.header, "Content-Type: application/json");
        post_req.header = http_add_field(post_req.header, tmp_buff1);
        post_req.data_field = http_add_field(post_req.data_field, tmp_buff2);

        if( dbg_flag & DBG_M2X ) {
            printf("\n\n\nupdate_stream_value\n", url);
            printf("sending to URL: %s\n", url);
            printf("Content-Type: application/json");
            printf("%s\n", tmp_buff1);
            printf("%s\n", tmp_buff2);
            }
        ret = http_post(&post_req, url);
        if( post_req.rx_msglen >0) {
            if( strcmp(post_req.rx_msg,"{\"status\":\"accepted\"}") &&  dbg_flag & DBG_M2X ) 
                printf("\nUNEXPECTED REPLY WAS: %s\n",post_req.rx_msg);
            }
        http_deinit(&post_req);
        }
    return ret;

}

int m2x_update_location ( const char *device_id_ptr, const char *api_key_ptr, float lat, float lng)
{
    int ret =0;
    if( doM2X ) {
	http_info_t put_req;
	char tmp_buff[64];
	char tmp_buff2[64];
	char url[256];

	memset(&put_req, 0, sizeof(http_info_t));
	memset(tmp_buff, 0, sizeof(tmp_buff));
	memset(tmp_buff2, 0, sizeof(tmp_buff2));
	memset(url, 0, sizeof(url));

	http_init(&put_req, 0);

	sprintf(url, "http://api-m2x.att.com/v2/devices/%s/location", device_id_ptr);
	sprintf(tmp_buff, "X-M2X-KEY:%s", api_key_ptr);
    sprintf(tmp_buff2, "{\"latitude\":%2.3f, \"longitude\":%2.3f}", lat, lng);

	put_req.header = http_add_field(put_req.header, tmp_buff);
	put_req.header = http_add_field(put_req.header, "Content-Type: application/json");
    put_req.data_field = http_add_field(put_req.data_field, tmp_buff2);    

        ret=http_put(&put_req, url);
        if( put_req.rx_msglen >0) {
            if( strcmp(put_req.rx_msg,"{\"status\":\"accepted\"}") &&  dbg_flag & DBG_M2X ) 
                printf("\nUNEXPECTED REPLY WAS: %s\n",put_req.rx_msg);
            }
	http_deinit(&put_req);
        }
    return ret;
}

int dataflow_get_token( const char *id, const char *secret)
{
    int ret = 0;
    http_info_t post_req;
    char body_buff[512], url[256];

    strncpy(df_id, id, sizeof(df_id));
    strncpy(df_secret, secret, sizeof(df_secret));

    memset(&post_req, 0, sizeof(http_info_t));
    memset(body_buff, 0, sizeof(body_buff));
    memset(url, 0, sizeof(url));

    http_init(&post_req, 0);

    strcpy(url, "https://api.dataflow.iot.att.com/v1/oauth2/token");
    sprintf(body_buff, "{"
                        "\"client_id\":\"%s\","
                        "\"client_secret\":\"%s\""
                        "}", id, secret);

    post_req.header = http_add_field(post_req.header, "Content-Type: application/json");
    post_req.data_field = http_add_field(post_req.data_field, body_buff);

    ret = http_post(&post_req, url);

    if( post_req.rx_msglen >0) {
        char *tok = strstr(post_req.rx_msg, "\"access_token\":");
        if (tok) {
            char *q1 = strstr(tok + 15, "\"");
            if (q1) {
                char *q2 = strstr(q1+1, "\"");
                if (q2) {
                    strncpy(auth_token, q1 + 1, q2 - q1 - 1);
                    printf("\nGot auth token %s\n", auth_token);
                };
            };
        } else {
            printf("\nDid not get auth token, response is %s\n",post_req.rx_msg);
            auth_token[0] = 0;
        }
    }
    http_deinit(&post_req);

    return ret;
}


int dataflow_create_object( const char *org, const char *project, const char *classid, const char *id)
{
    int ret = 0;
    http_info_t put_req;
    char auth_buff[256], body_buff[256], url[256];
    char body[] = "{}";

    if (!auth_token[0]) {
        printf("Failed to obtain auth token, not ingesting to DataFlow");
        return -1;
    }
    
    memset(&put_req, 0, sizeof(http_info_t));
    memset(auth_buff, 0, sizeof(auth_buff));
    memset(url, 0, sizeof(url));
    
    http_init(&put_req, 0);

    sprintf(url, "https://api.dataflow.iot.att.com/v1/objects/%s/%s/%s/%s", org, project, classid, id);
    sprintf(auth_buff, "Authorization: Bearer %s", auth_token);

    put_req.header = http_add_field(put_req.header, "Content-Type: application/json");
    put_req.header = http_add_field(put_req.header, auth_buff);
    put_req.data_field = http_add_field(put_req.data_field, body);

    ret = http_put(&put_req, url);

    long response_code;
    curl_easy_getinfo(put_req.curl, CURLINFO_RESPONSE_CODE, &response_code);
    http_deinit(&put_req);    

    if (response_code == 401) {
        dataflow_get_token(df_id, df_secret);
        return dataflow_create_object(org, project, classid, id);
    };

    return ret;    
}


int dataflow_ingest_data( const char *org, const char *project, const char *component, 
                          const char *cls, const char *id, 
                          float light, float temp, char **accel, const char *sig_strength,
                          int gps, float lat, float lng)
{
    int ret = 0;
    http_info_t post_req;
    char auth_buff[256], body_buff[256], url[256], gps_buff[128];

    if (!auth_token[0]) {
        printf("Failed to obtain auth token, not ingesting to DataFlow");
        return -1;
    }
    
    memset(&post_req, 0, sizeof(http_info_t));
    memset(auth_buff, 0, sizeof(auth_buff));
    memset(body_buff, 0, sizeof(body_buff));
    memset(url, 0, sizeof(url));
    
    http_init(&post_req, 0);

    sprintf(url, "https://api.dataflow.iot.att.com/v1/messages/%s/%s/%s", org, project, component);
    sprintf(auth_buff, "Authorization: Bearer %s", auth_token);
    if (gps)
       sprintf(gps_buff, ",\"gps\":true,\"location\":{\"latitude\":%2.3f, \"longitude\":%2.3f}", lat, lng);
    else
       strcpy(gps_buff, ",\"gps\":false");

    sprintf(body_buff, "{"
                        "\"class\":\"%s\","
                        "\"object\":\"%s\","
                        "\"data\":{"
                            "\"temperature\":%.2f,"
                            "\"light\":%.3f,"
                            "\"accelX\":%.0f,"
                            "\"accelY\":%.0f,"
                            "\"accelZ\":%.0f,"
                            "\"signal\":%s"
                            "%s"
                            "}"
                        "}", cls, id, temp, light, atof(accel[0]), atof(accel[1]), atof(accel[2]), sig_strength, gps_buff);
/*
    printf("Body is \n%s\n", body_buff);
    fflush(stdout);
*/
    post_req.header = http_add_field(post_req.header, "Content-Type: application/json");
    post_req.header = http_add_field(post_req.header, auth_buff);
    post_req.data_field = http_add_field(post_req.data_field, body_buff);

    ret = http_post(&post_req, url);
    if( post_req.rx_msglen >0) {
        if( strcmp(post_req.rx_msg,"{\"status\":\"accepted\"}")) 
            printf("\nUNEXPECTED REPLY WAS: %s\n",post_req.rx_msg);
    };

    long response_code;
    curl_easy_getinfo(post_req.curl, CURLINFO_RESPONSE_CODE, &response_code);
    http_deinit(&post_req);    

    if (response_code == 401) {
        dataflow_get_token(df_id, df_secret);
        return dataflow_ingest_data(org, project, component, 
                                    cls, id, 
                                    light, temp, accel, sig_strength, 
                                    gps, lat, lng);
    };

    return ret;
}


int udp_send(const char *host, const char *port, const char* imei, float light, float temp, char **accel, int gps, float lat, float lng, const char* sig_strength)
{
    char body_buff[128];

    int len, udpsocket;
    struct addrinfo hints;
    struct addrinfo *dest; 

    if (lng == 1000)
       return -1;

    snprintf(body_buff, sizeof(body_buff), "'%s',%.2f,%.3f,%.0f,%.0f,%.0f,%2.3f,%2.3f,%d", 
            imei, 
            temp, light, 
            atof(accel[0]), atof(accel[1]), atof(accel[2]),
            lat, lng,
            atoi(sig_strength));

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    int err = getaddrinfo(host, port, &hints, &dest);
    if (err) {
        printf("Cannot resolve UDP address");
        return -1;
    }

    if ((udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("Cannot create UDP socket");
        return -1;
    };

    if ((len = sendto(udpsocket, body_buff, strlen(body_buff), 0, (struct sockaddr *) dest->ai_addr, dest->ai_addrlen)) == -1) 
            printf("Failed to send UDP message");

    close(udpsocket);               

    return len;
}


size_t static write_callback_func(void *buffer, size_t size, size_t nmemb, void *userp)
{
    char *response_ptr =  (char*)(userp+rsize);

    if( dbg_flag & DBG_FLOW )
        printf("-FLOW: IN CALLBACK: usr=%08X buffer(%d/%d)=%s\n",response_ptr,size*nmemb,rsize,buffer);

    if( size*nmemb > 0 ) {
        memcpy(response_ptr, buffer, (size*nmemb));
        }

    rsize += (size * nmemb);
    response_ptr[rsize] = '\0';
    return(size * nmemb);
}

int http_get(http_info_t *http_req, const char *url, char *response)
{
    CURLcode res;

    if ((http_req == NULL) || (url == NULL)) {
        return -1;
        }

    if (http_req->curl == NULL) 
        return -2;

    curl_easy_setopt(http_req->curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(http_req->curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(http_req->curl, CURLOPT_WRITEFUNCTION, write_callback_func);
    curl_easy_setopt(http_req->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(http_req->curl, CURLOPT_FOLLOWLOCATION, 1); 
    curl_easy_setopt(http_req->curl, CURLOPT_URL, url);
    rsize = 0;

    if (http_req->header != NULL)
        curl_easy_setopt(http_req->curl, CURLOPT_HTTPHEADER, http_req->header);

    http_req->last_data = http_req->data_field;
    http_req->total_len = http_field_len(http_req->data_field);
    if (http_req->total_len >= 0)
        curl_easy_setopt(http_req->curl, CURLOPT_POSTFIELDSIZE, http_req->total_len);

    res = curl_easy_perform(http_req->curl);
    return (res != CURLE_OK) ? -res : 0;
}



//
// This function is called to solicit an LED command from Flow based on accelerator and temp/humid data 
//

char *flow_get ( const char *flow_base_url, const char *flow_input_name, 
                 const char *flow_device_name, const char *flow_server, 
                 const char *get_cmd, char *response, int resp_size)
{
    int r;
    http_info_t get_req;
    char tmp_buff1[256];
    char url[256];

    if( dbg_flag & DBG_FLOW )
        printf("-FLOW: BASE URL: %s\n-FLOW: INPUT NAME: %s\n-FLOW: DEVICE: %s\n-FLOW: COMMAND: %s\n",
                flow_base_url, flow_input_name, flow_device_name, get_cmd);

    memset(response, 0, resp_size);
    memset(&get_req, 0, sizeof(http_info_t));
    memset(tmp_buff1, 0, sizeof(tmp_buff1));
    memset(url, 0, sizeof(url));

    http_init(&get_req, 1);

    sprintf(url, "%s/%s?serial=%s%s", flow_base_url, flow_input_name, flow_device_name, get_cmd);
    get_req.header = http_add_field(get_req.header, tmp_buff1);

    sprintf(tmp_buff1, "Host:%s", flow_server);
    get_req.header = http_add_field(get_req.header, tmp_buff1);
    get_req.header = http_add_field(get_req.header, "HTTP/1.1");
    get_req.header = http_add_field(get_req.header, "Accept: */*");

    do {
        r=http_get(&get_req, url, response);
        if (r < 0) {
            if( dbg_flag & DBG_FLOW )
                printf("-FLOW: bad response %d, wait 30- seconds\n",r);
            sleep(30);
            }
        }
    while( r );

    http_deinit(&get_req);
    return response;
}


//
// This function is called to update a LED COLOR value.
//
//curl -i -X PUT https://api-m2x.att.com/v2/devices/2ac9dc89132469eb809bea6e3a95d675/streams/rgb/value 
//     -H "X-M2X-KEY: 6cd9c60f4a4e5d91d0ec4cc79536c661" 
//     -H "Content-Type: application/json" 
//     -d "{ \"value\": \"WHITE\" }"

int m2x_update_color_value ( const char *device_id_ptr, const char *api_key_ptr, const char *stream_name_ptr, const char *stream_value_ptr)
{
    http_info_t put_req;
    char tmp_buff1[256], tmp_buff2[256], tmp_buff3[64];;
    char url[256];

    memset(&put_req, 0, sizeof(http_info_t));
    memset(tmp_buff1, 0, sizeof(tmp_buff1));
    memset(tmp_buff2, 0, sizeof(tmp_buff2));
    memset(url, 0, sizeof(url));

    http_init(&put_req, 1);

    sprintf(url, "http://api-m2x.att.com/v2/devices/%s/streams/%s/value", device_id_ptr, stream_name_ptr);
    sprintf(tmp_buff1, "X-M2X-KEY: %s", api_key_ptr);
    sprintf(tmp_buff2, "{ \"value\": \"%s\" }", stream_value_ptr);

    put_req.header = http_add_field(put_req.header, "Content-Type: application/json");
    put_req.header = http_add_field(put_req.header, tmp_buff1);
    put_req.data_field = http_add_field(put_req.data_field, tmp_buff2);

    if (http_put(&put_req, url) < 0)
	return -1;

    http_deinit(&put_req);
    return 0;
}


//
//=========================================================================
//
// This function is called to create a device
//
int m2x_create_device ( const char *api_key_ptr, const char *device_name_ptr,  char * ret_buffer )
{
    int ret = 0;

    if( doM2X ) {
        http_info_t post_req;
        char tmp_buff1[256], tmp_buff2[512];
        char url[256];
        time_t t = time(NULL);;
        struct tm *tm;

        memset(&post_req, 0, sizeof(http_info_t));
        memset(tmp_buff1, 0, sizeof(tmp_buff1));
        memset(tmp_buff2, 0, sizeof(tmp_buff2));
        memset(url, 0, sizeof(url));

        http_init(&post_req, 0);

        sprintf(url, "http://api-m2x.att.com/v2/devices" );
        sprintf(tmp_buff1, "X-M2X-KEY: %s", api_key_ptr);
        sprintf(tmp_buff2, "{ \"name\": \"Global Starter Kit\", \"serial\" : \"%s\", \"description\": \"Demo Device\", "
                             "\"tags\": \"QSTA\", \"visibility\":\"private\" }",  device_name_ptr);

        post_req.header = http_add_field(post_req.header, "Content-Type: application/json");
        post_req.header = http_add_field(post_req.header, tmp_buff1);
        post_req.data_field = http_add_field(post_req.data_field, tmp_buff2);

        ret = http_post(&post_req, url);
        if( post_req.rx_msglen >0) 
            strcpy(ret_buffer,post_req.rx_msg);
        http_deinit(&post_req);
        }
    return ret;
}


void m2x_list_devices ( const char *api_key_ptr, char *ret_buffer)
{
    http_info_t get_req;
    char tmp_buff1[256];
    char *url = "http://api-m2x.att.com/v2/devices";
    int  i;

    memset(&get_req, 0, sizeof(http_info_t));
    http_init(&get_req, 1);

    sprintf(tmp_buff1, "X-M2X-KEY: %s", api_key_ptr);
    get_req.header = http_add_field(get_req.header, "Accept: */*");
    get_req.header = http_add_field(get_req.header, "Content-Type: application/json");
    get_req.header = http_add_field(get_req.header, tmp_buff1);


    do {
        i=http_get(&get_req, url, ret_buffer);
        if (i < 0) 
            sleep(30);
        }
    while( i );

    http_deinit(&get_req);
}


void m2x_device_info ( const char *api_key_ptr, char *iccid_ptr, char *ret_buffer)
{
    http_info_t get_req;
    char tmp_buff1[256];
    char *url = "http://api-m2x.att.com/v2/devices";
    int  i;

    memset(&get_req, 0, sizeof(http_info_t));
    http_init(&get_req, 1);

    sprintf(tmp_buff1, "X-M2X-KEY: %s", api_key_ptr);
    get_req.header = http_add_field(get_req.header, "Accept: */*");
    get_req.header = http_add_field(get_req.header, "Content-Type: application/json");
    get_req.header = http_add_field(get_req.header, tmp_buff1);

    sprintf(tmp_buff1, "http://api-m2x.att.com/v2/devices/search?serial=%s", iccid_ptr);

    do {
        i=http_get(&get_req, tmp_buff1, ret_buffer);
        if (i < 0) 
            sleep(30);
        }
    while( i );

    http_deinit(&get_req);
}

