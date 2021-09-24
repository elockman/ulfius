/**
 * 
 * Xeal WebUI, using Ulfius Framework
 * 
 * AUTHOR: Eric Lockman
 *   DATE: 09/13/2021
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <string.h>
// #include <jansson.h>

#include <ulfius.h>
#include <u_example.h>

#include "static_compressed_inmemory_website_callback.h"
#include "http_compression_callback.h"

#define PORT 8080
// #define PREFIX "/sheep"
// #define FILE_PREFIX "/upload"
#define FILE_PREFIX "/info"
#define STATIC_FOLDER "static"


#define DEBUG_LVL 1

#define PRINT_RESP 1
#define GPIO_ENABLE 0
#define ADC_ENABLE 0
#define JSON_ENABLE 1

#if JSON_ENABLE
#include <cJSON.h>
#endif

// #define CONFIG_FILE "/user-data/profile/config/config.json"
#define CONFIG_FILE "config.json"

#define ADC_CHAN_MODEL    10
#define ADC_CHAN_VERSION  9
#define ADC_CHAN_VIN      0

#define PAGE_SIZE      8000
#define LINE_SIZE      256

int PinBtn;
int PinLedRed;
int PinLedGrn;
int PinLedBlu;
int PinMeshPwr;
int PinWifiPwr;

int HwModel=0;
int HwVersion=0;
int VoltageInput=0;

char HttpResp[100];
char HttpPage[PAGE_SIZE];

#if GPIO_ENABLE
// return port-pin index
int gpio_port_a(int pin)
{
  pin = pin + 0;
  return pin;
}

// return port-pin index
int gpio_port_b(int pin)
{
  pin = pin + 32;
  return pin;
}

// return port-pin index
int gpio_port_c(int pin)
{
  pin = pin + 64;
  return pin;
}

// return port-pin index
int gpio_port_d(int pin)
{
  pin = pin + 96;
  return pin;
}

// return port-pin name based on index
void gpio_get_port_pin(int index, char* port_name)
{
  if(index >= 96) {
    sprintf(port_name, "PD%d", index-96);
  }
  else if(index >= 64) {
    sprintf(port_name, "PC%d", index-64);
  }
  else if(index >= 32) {
    sprintf(port_name, "PB%d", index-32);
  }
  else {
    sprintf(port_name, "PA%d", index);
  }
}

void gpio_configure_pin(int index, bool output, bool active_low)
{
  char port_pin[5];
  char cmdbuf[100];
  FILE* fp;

  //assign port-pin string based on index
  gpio_get_port_pin(index, port_pin);

  //create port definintion
  fp = fopen("/sys/class/gpio/export", "w");
  fprintf(fp, "%d", index);
  fclose(fp);

  //set pin direction
  sprintf(cmdbuf, "/sys/class/gpio/%s/direction", port_pin);
  fp = fopen(cmdbuf, "w");
  if(output) {
      fprintf(fp, "out");
  } else {
      fprintf(fp, "in");
  }
  fclose(fp);
  
  //set pin active level
  sprintf(cmdbuf, "/sys/class/gpio/%s/active_low", port_pin);
  fp = fopen(cmdbuf, "w");
  if(active_low) {
      fprintf(fp, "1");
  } else {
      fprintf(fp, "0");
  }
  fclose(fp);
  
}

void gpio_cmd_output(int index, bool value)
{
  char port_pin[5];
  char cmdbuf[100];
  FILE* fp;

  //assign port-pin string based on index
  gpio_get_port_pin(index, port_pin);

  //set pin value
  sprintf(cmdbuf, "/sys/class/gpio/%s/value", port_pin);
  fp = fopen(cmdbuf, "w");
  if(value) {
      fprintf(fp, "1");
  } else {
      fprintf(fp, "0");
  }
  fclose(fp);
}

bool gpio_get_input(int index)
{
  bool retval = false;
  FILE* fp;
  char port_pin[5];
  char cmdbuf[100];
  int value = 0;

  //assign port-pin string based on index
  gpio_get_port_pin(index, port_pin);

  //set pin value
  sprintf(cmdbuf, "/sys/class/gpio/%s/value", port_pin);
  fp = fopen(cmdbuf, "r");
  if(fp == NULL){
    //failed to open file, return false
    return retval;
  }
  fscanf (fp, "%d", &value);
  fclose(fp);
  
  if(value==0) {
    retval = false;
  } else {
    retval = true;
  }

  //printf("%s VAL %d with ActLvl %d\n", port_pin, value, level);
  return retval;
}

void gpio_init(void)
{
  PinBtn = gpio_port_b(25);
  PinLedRed = gpio_port_c(24);
  PinLedGrn = gpio_port_c(15);
  PinLedBlu = gpio_port_c(10);
  PinMeshPwr = gpio_port_c(25);
  PinWifiPwr = gpio_port_c(26);

  gpio_configure_pin(PinBtn, false, true);
  gpio_configure_pin(PinLedRed, true, true);
  gpio_configure_pin(PinLedGrn, true, true);
  gpio_configure_pin(PinLedBlu, true, true);
  gpio_configure_pin(PinMeshPwr, true, false);
  gpio_configure_pin(PinWifiPwr, true, false);
}


void adc_enable(int chan)
{
#if ADC_ENABLE
  char cmdbuf[100];
  FILE* fp;

  //set pin value
  sprintf(cmdbuf, "/sys/bus/iio/devices/iio:device0/scan_elements/in_voltage%d_en", chan);
  fp = fopen(cmdbuf, "w");
  fprintf(fp, "1");
  fclose(fp);
#endif
}

void adc_disable(int chan)
{
#if ADC_ENABLE
  char cmdbuf[100];
  FILE* fp;

  //set pin value
  sprintf(cmdbuf, "/sys/bus/iio/devices/iio:device0/scan_elements/in_voltage%d_en", chan);
  fp = fopen(cmdbuf, "w");
  fprintf(fp, "0");
  fclose(fp);
#endif
}

void adc_write_scale(int scale)
{
#if ADC_ENABLE
  FILE* fp;
  char cmdbuf[100];

  //set scale value
  sprintf(cmdbuf, "/sys/bus/iio/devices/iio:device0/in_voltage_scale");
  fp = fopen(cmdbuf, "w");
  if(fp == NULL){
    //failed to open file, return false
    return;
  }
  fprintf(fp, "%d", scale);
  fclose(fp);
#endif
}

float adc_read_scale(void)
{
  float value = 0.0;
#if ADC_ENABLE
  FILE* fp;
  char cmdbuf[100];

  //set pin value
  sprintf(cmdbuf, "/sys/bus/iio/devices/iio:device0/in_voltage_scale");
  fp = fopen(cmdbuf, "r");
  if(fp == NULL){
    //failed to open file, return false
    return value;
  }
  fscanf (fp, "%f", &value);
  fclose(fp);

  printf("Scale is %f\n", value);
#endif
  return value;
}

int adc_read_raw(int chan)
{
  int value = 0;
#if ADC_ENABLE
  FILE* fp;
  char cmdbuf[100];

  //set pin value
  sprintf(cmdbuf, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", chan);
  fp = fopen(cmdbuf, "r");
  if(fp == NULL){
    //failed to open file, return false
    return -1;
  }
  fscanf (fp, "%d", &value);
  fclose(fp);
#endif

  return value;
}


float adc_read_mv(int chan)
{
    float value = adc_read_scale() * adc_read_raw(chan);
    printf("mV is %f\n", value);
    return value;
}

void adc_init()
{
#if ADC_ENABLE
  adc_enable(ADC_CHAN_MODEL);
  adc_enable(ADC_CHAN_VERSION);
  adc_enable(ADC_CHAN_VIN);
#endif
}

#endif

void reboot(void)
{
  system("reboot");
}

void announce(void)
{
#if GPIO_ENABLE
  int i = 0;
  for(i=0;i<1000;i++) {
    gpio_cmd_output(PinLedRed, 1);
    gpio_cmd_output(PinLedGrn, 1);
    gpio_cmd_output(PinLedBlu, 1);
    gpio_cmd_output(PinLedRed, 0);
    gpio_cmd_output(PinLedGrn, 0);
    gpio_cmd_output(PinLedBlu, 0);
  }
#endif
}


#if JSON_ENABLE
bool read_config_param(char* group, char* param, char* value)
{
  bool retval = false;
  FILE* fp = fopen(CONFIG_FILE, "r");
  char buf[4096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  cJSON* root = cJSON_Parse(buf);
  
  cJSON* conf_obj = cJSON_GetObjectItem(root, "configuration");
  if(conf_obj){
    cJSON* group_obj = cJSON_GetObjectItem(conf_obj, group);
    cJSON* group_param = cJSON_GetObjectItem(group_obj, param);
    sprintf(value, "%s", cJSON_GetStringValue(group_param));
  }
  cJSON_Delete(root);

#if DEBUG_LVL >= 1
  printf("read_config_param: config/%s/%s = %s\n", group, param, value);
#endif

  return retval;
}

void write_config_param(char* group, char* param, char* value)
{
  FILE *fp = NULL;
  fp = fopen(CONFIG_FILE, "r");
  char buf[4096] = {0};
  char *out;
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  cJSON* root = cJSON_Parse(buf);
  out = cJSON_Print(root);
//   printf("before modify:%s\n",out);

  cJSON* conf_obj = cJSON_GetObjectItem(root, "configuration");
  if(conf_obj){
    cJSON* group_obj = cJSON_GetObjectItem(conf_obj, group);
    cJSON* group_param = cJSON_GetObjectItem(group_obj, param);
    cJSON_SetValuestring(group_param, value);
    out = cJSON_Print(root);
//     printf("after modify:%s\n",out);

    fp = fopen(CONFIG_FILE,"w");
    if(fp == NULL) {
      fprintf(stderr,"open file failed\n");
      return;
    }
    fprintf(fp,"%s",out);
    fclose(fp);
    free(out);
    cJSON_Delete(root);
  }
}
#endif


/**
 * decode a u_map into a string
 */
char * print_map(const struct _u_map * map) {
  char * line, * to_return = NULL;
  const char **keys, * value;
  int len, i;
  if (map != NULL) {
    keys = u_map_enum_keys(map);
    for (i=0; keys[i] != NULL; i++) {
      value = u_map_get(map, keys[i]);
      len = snprintf(NULL, 0, "key is %s, length is %zu, value is %.*s", keys[i], u_map_get_length(map, keys[i]), (int)u_map_get_length(map, keys[i]), value);
      line = o_malloc((len+1)*sizeof(char));
      snprintf(line, (len+1), "key is %s, length is %zu, value is %.*s", keys[i], u_map_get_length(map, keys[i]), (int)u_map_get_length(map, keys[i]), value);
      if (to_return != NULL) {
        len = o_strlen(to_return) + o_strlen(line) + 1;
        to_return = o_realloc(to_return, (len+1)*sizeof(char));
        if (o_strlen(to_return) > 0) {
          strcat(to_return, "\n");
        }
      } else {
        to_return = o_malloc((o_strlen(line) + 1)*sizeof(char));
        to_return[0] = 0;
      }
      strcat(to_return, line);
      o_free(line);
    }
    return to_return;
  } else {
    return NULL;
  }
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/led?clr=blue&val=0
 * returns { param : "value"}
 */
int callback_led_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  json_t * json_body = NULL;
  char clr[24];
  char val[24];
  
  sprintf(clr, "%s", u_map_get(request->map_url, "clr"));
  sprintf(val, "%s", u_map_get(request->map_url, "val"));
  json_body = json_object();

#if GPIO_ENABLE
  int state = atoi(val);
#endif
  if(!strcmp("red",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedRed, 1);
    }else{
      gpio_cmd_output(PinLedRed, 0);
    }
#endif
    json_object_set_new(json_body, "red", json_string(val));
  }else if(!strcmp("green",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedBlu, 1);
    }else{
      gpio_cmd_output(PinLedBlu, 0);
    }
#endif
    json_object_set_new(json_body, "green", json_string(val));
  }else if(!strcmp("blue",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedGrn, 1);
    }else{
      gpio_cmd_output(PinLedGrn, 0);
    }
#endif
    json_object_set_new(json_body, "blue", json_string(val));
  }
  ulfius_set_json_body_response(response, 200, json_body);
  json_decref(json_body);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/wifi?val=1
 * returns { param : "value"}
 */
int callback_wifi_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  json_t * json_body = NULL;
  char val[24];

  sprintf(val, "%s", u_map_get(request->map_url, "val"));
  json_body = json_object();

#if GPIO_ENABLE
  int state = atoi(val);
  if(state){
    gpio_cmd_output(PinWifiPwr, 1);
  }else{
    gpio_cmd_output(PinWifiPwr, 0);
  }
#endif

  json_object_set_new(json_body, "wifi", json_string(val));
  ulfius_set_json_body_response(response, 200, json_body);
  json_decref(json_body);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/mesh?val=1
 * returns { param : "value"}
 */
int callback_mesh_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  json_t * json_body = NULL;
  char val[24];

  sprintf(val, "%s", u_map_get(request->map_url, "val"));
  json_body = json_object();

#if GPIO_ENABLE
  int state = atoi(val);
  if(state){
    gpio_cmd_output(PinMeshPwr, 1);
  }else{
    gpio_cmd_output(PinMeshPwr, 0);
  }
#endif

  json_object_set_new(json_body, "mesh", json_string(val));
  ulfius_set_json_body_response(response, 200, json_body);
  json_decref(json_body);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/adc?chan=vin
 * returns { param : "value"}
 */
int callback_adc_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  json_t * json_body = NULL;
  char chan[24];
  char val[24];
  int adc = 0;

  sprintf(chan, "%s", u_map_get(request->map_url, "chan"));
  json_body = json_object();

  if(!strcmp("model",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_MODEL);
#endif
    sprintf(val, "%d", adc);
    json_object_set_new(json_body, "model", json_string(val));
  }else if(!strcmp("hw",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_VERSION);
#endif
    sprintf(val, "%d", adc);
    json_object_set_new(json_body, "hw", json_string(val));
  }else if(!strcmp("vin",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_VIN);
#endif
    sprintf(val, "%d", adc);
    json_object_set_new(json_body, "vin", json_string(val));
  }else if(!strcmp("pb",chan)){
#if ADC_ENABLE
    adc = (int)gpio_get_input(PinBtn));
#endif
    sprintf(val, "%d", adc);
    json_object_set_new(json_body, "pb", json_string(val));
  }
  ulfius_set_json_body_response(response, 200, json_body);
  json_decref(json_body);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:80/cfg?group=xxx&param=yyy
 * returns { param : "value"}
 */
int callback_read_config_param (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char group[24];
  char param[24];
  char value[24];

  sprintf(group, "%s", u_map_get(request->map_url, "group"));
  sprintf(param, "%s", u_map_get(request->map_url, "param"));
  read_config_param(group, param, value);

//   json_t * json_body = NULL;
//   json_body = json_object();
//   json_object_set_new(json_body, param, json_string(value));
//   ulfius_set_json_body_response(response, 200, json_body);
//   json_decref(json_body);

  char * resp_body = NULL;
  sprintf(resp_body, "{\"%s\":\"%s\"}", param, value);  
  ulfius_set_cjson_body_response(response, 200, resp_body);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Write config param to config.json file
 * URL format:   http://localhost:8080/cfg_set?group=xxx&param=yyy&value=zzz
 * returns { param : "value"}
 */
int callback_write_config_param (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char group[24];
  char param[24];
  char value[24];

  sprintf(group, "%s", u_map_get(request->map_url, "group"));
  sprintf(param, "%s", u_map_get(request->map_url, "param"));
  sprintf(value, "%s", u_map_get(request->map_url, "value"));
  write_config_param(group, param, value);

//   json_t * json_body = NULL;
//   json_body = json_object();
//   json_object_set_new(json_body, param, json_string(value));
//   ulfius_set_json_body_response(response, 200, json_body);
//   json_decref(json_body);

  char * resp_body = NULL;
  sprintf(resp_body, "{\"%s\":\"%s\"}", param, value);  
  ulfius_set_cjson_body_response(response, 200, resp_body);

  return U_CALLBACK_CONTINUE;
}

/**
 * info page
 */
static int callback_info_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/info.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * control page
 */
static int callback_control_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/control.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * config page
 */
static int callback_config_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/config.html", "r");
  char buf[16096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * admin page
 */
static int callback_admin_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/admin.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * neighbors page
 */
static int callback_neighbors_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/neighbors.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * sensors page
 */
static int callback_sensors_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/sensors.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * logging page
 */
static int callback_logging_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/logging.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * login page
 */
static int callback_login_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/login.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * logout page
 */
static int callback_logout_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/logout.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * test page
 */
static int callback_test_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  FILE* fp = fopen("./static/test.html", "r");
  char buf[8096] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}


/**
 * Main function
 */
int main (int argc, char **argv) {
  struct _u_compressed_inmemory_website_config file_config;
  
  // Initialize the instance
  struct _u_instance instance;
  
  y_init_logs("xeal_webui", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Starting Xeal WebUI code");
  
  if (u_init_compressed_inmemory_website_config(&file_config) == U_OK) {
    u_map_put(&file_config.mime_types, ".html", "text/html");
    u_map_put(&file_config.mime_types, ".css", "text/css");
    u_map_put(&file_config.mime_types, ".js", "application/javascript");
    u_map_put(&file_config.mime_types, ".png", "image/png");
    u_map_put(&file_config.mime_types, ".jpg", "image/jpeg");
    u_map_put(&file_config.mime_types, ".jpeg", "image/jpeg");
    u_map_put(&file_config.mime_types, ".ttf", "font/ttf");
    u_map_put(&file_config.mime_types, ".woff", "font/woff");
    u_map_put(&file_config.mime_types, ".woff2", "font/woff2");
    u_map_put(&file_config.mime_types, ".map", "application/octet-stream");
    u_map_put(&file_config.mime_types, ".json", "application/json");
    u_map_put(&file_config.mime_types, "*", "application/octet-stream");
    file_config.files_path = "static";
    file_config.url_prefix = FILE_PREFIX;

    if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_init_instance, abort");
      return(1);
    }
    
    // Max post param size is 16 Kb, which means an uploaded file is no more than 16 Kb
    instance.max_post_param_size = 16*1024;
    
//     if (ulfius_set_upload_file_callback_function(&instance, &file_upload_callback, "my cls") != U_OK) {
//       y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_set_upload_file_callback_function");
//     }
    
    // Endpoint list declaration
    // The first 3 are webservices with a specific url
    // The last endpoint will be called for every GET call and will serve the static files
//     ulfius_add_endpoint_by_val(&instance, "POST", PREFIX, NULL, 1, &callback_sheep_counter_start, &nb_sheep);
//     ulfius_add_endpoint_by_val(&instance, "PUT", PREFIX, NULL, 1, &callback_sheep_counter_add, &nb_sheep);
//     ulfius_add_endpoint_by_val(&instance, "DELETE", PREFIX, NULL, 1, &callback_sheep_counter_reset, &nb_sheep);
//     ulfius_add_endpoint_by_val(&instance, "*", PREFIX, NULL, 2, &callback_http_compression, NULL);
//     ulfius_add_endpoint_by_val(&instance, "*", STATIC_FOLDER, "/upload", 1, &callback_upload_file, NULL);
//     ulfius_add_endpoint_by_val(&instance, "*", STATIC_FOLDER, "/submit", 1, &callback_form_submit, NULL);
//     ulfius_add_endpoint_by_val(&instance, "GET", "/test", NULL, 1, &callback_test, NULL);
//     ulfius_add_endpoint_by_val(&instance, "GET", "/tests", NULL, 1, &callback_get_counts, NULL);
    
    ulfius_add_endpoint_by_val(&instance, "GET", "/info", NULL, 1, &callback_info_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/control", NULL, 1, &callback_control_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/config", NULL, 1, &callback_config_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/admin", NULL, 1, &callback_admin_page, NULL);

    ulfius_add_endpoint_by_val(&instance, "GET", "/neighbors", NULL, 1, &callback_neighbors_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/sensors", NULL, 1, &callback_sensors_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/logging", NULL, 1, &callback_logging_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/login", NULL, 1, &callback_login_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/logout", NULL, 1, &callback_logout_page, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/test", NULL, 1, &callback_test_page, NULL);

    ulfius_add_endpoint_by_val(&instance, "GET", "/led", NULL, 1, &callback_led_control, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/wifi", NULL, 1, &callback_wifi_control, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/mesh", NULL, 1, &callback_mesh_control, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/adc", NULL, 1, &callback_adc_control, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/cfg", NULL, 1, &callback_read_config_param, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/cfg_set", NULL, 1, &callback_write_config_param, NULL);
//     ulfius_add_endpoint_by_val(&instance, "GET", "*", NULL, 1, &callback_static_compressed_inmemory_website, &file_config);
    
    // Start the framework
    if (ulfius_start_framework(&instance) == U_OK) {
      printf("Start Xeal WebUI on port %u\n", instance.port);
      
      // Wait for the user to press <enter> on the console to quit the application
      getchar();
    } else {
      printf("Error starting framework\n");
    }

    printf("End framework\n");
    ulfius_stop_framework(&instance);
    ulfius_clean_instance(&instance);
    u_clean_compressed_inmemory_website_config(&file_config);
  }
  
  y_close_logs();
  
  return 0;
}
