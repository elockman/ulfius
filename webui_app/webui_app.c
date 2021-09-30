/**
 * 
 * WebUI, using Ulfius Framework
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

#include <ulfius.h>
#include "static_compressed_inmemory_website_callback.h"
#include "http_compression_callback.h"

#define RELEASE_BUILD  1
// #define DEV_BUILD      1
// #define LOCAL_PC_BUILD 1

#if RELEASE_BUILD==1
  #define DEBUG_LVL 0
  #define PORT 80
  #define CONFIG_FILE "/user-data/profile/config/config.json"
  #define STATIC_PATH "/user-data/xsite/webui/static/"
  #define GPIO_ENABLE 1
  #define ADC_ENABLE 0
  #define JSON_ENABLE 1
#elif DEV_BUILD==1
  #define DEBUG_LVL 1
  #define PORT 8080
  #define CONFIG_FILE "config.json"
  #define STATIC_PATH "./static/"
  //#define STATIC_PATH "/test/ulfius/example_programs/webui_app/static/"
  #define GPIO_ENABLE 1
  #define ADC_ENABLE 0
  #define JSON_ENABLE 1
#elif LOCAL_PC_BUILD==1
  #define DEBUG_LVL 1
  #define PORT 8080
  #define CONFIG_FILE "config.json"
  #define STATIC_PATH "./static/"
  #define GPIO_ENABLE 0
  #define ADC_ENABLE 0
  #define JSON_ENABLE 1
#endif

#if JSON_ENABLE
#include <cJSON.h>
#endif

#define FILE_PREFIX "/info"

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
  char cmd[] = "reboot";
  system(cmd);
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

  cJSON* conf_obj = cJSON_GetObjectItem(root, "configuration");
  if(conf_obj){
    cJSON* group_obj = cJSON_GetObjectItem(conf_obj, group);
    cJSON* group_param = cJSON_GetObjectItem(group_obj, param);
    cJSON_SetValuestring(group_param, value);
    out = cJSON_Print(root);

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


// /**
//  * decode a u_map into a string
//  */
// char * print_map(const struct _u_map * map) {
//   char * line, * to_return = NULL;
//   const char **keys, * value;
//   int len, i;
//   if (map != NULL) {
//     keys = u_map_enum_keys(map);
//     for (i=0; keys[i] != NULL; i++) {
//       value = u_map_get(map, keys[i]);
//       len = snprintf(NULL, 0, "key is %s, length is %zu, value is %.*s", keys[i], u_map_get_length(map, keys[i]), (int)u_map_get_length(map, keys[i]), value);
//       line = o_malloc((len+1)*sizeof(char));
//       snprintf(line, (len+1), "key is %s, length is %zu, value is %.*s", keys[i], u_map_get_length(map, keys[i]), (int)u_map_get_length(map, keys[i]), value);
//       if (to_return != NULL) {
//         len = o_strlen(to_return) + o_strlen(line) + 1;
//         to_return = o_realloc(to_return, (len+1)*sizeof(char));
//         if (o_strlen(to_return) > 0) {
//           strcat(to_return, "\n");
//         }
//       } else {
//         to_return = o_malloc((o_strlen(line) + 1)*sizeof(char));
//         to_return[0] = 0;
//       }
//       strcat(to_return, line);
//       o_free(line);
//     }
//     return to_return;
//   } else {
//     return NULL;
//   }
// }


/**
 * set_cjson_body_response
 * Add a cjson body to a response
 * return U_OK on success
 */
int set_cjson_body_response(struct _u_response * response, const unsigned int status, cJSON *json) {
  if (response != NULL && json != NULL) {
    o_free(response->binary_body);
    response->binary_body = cJSON_Print(json);
    response->binary_body_length = o_strlen((char*)response->binary_body);
    response->status = status;

    u_map_put(response->map_header, ULFIUS_HTTP_HEADER_CONTENT, ULFIUS_HTTP_ENCODING_JSON);
    return U_OK;
  } else {
    return U_ERROR_PARAMS;
  }
}


/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/led?clr=blue&val=0
 * returns { param : "value"}
 */
int callback_led_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char resp_body[256];
  char clr[24];
  char val[24];
  
  sprintf(clr, "%s", u_map_get(request->map_url, "clr"));
  sprintf(val, "%s", u_map_get(request->map_url, "val"));

  int state = atoi(val);

  if(!strcmp("red",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedRed, 1);
    }else{
      gpio_cmd_output(PinLedRed, 0);
    }
#endif
    sprintf(resp_body, "{\"red\":\"%d\"}", state);
  }else if(!strcmp("green",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedGrn, 1);
    }else{
      gpio_cmd_output(PinLedGrn, 0);
    }
#endif
    sprintf(resp_body, "{\"green\":\"%d\"}", state);
  }else if(!strcmp("blue",clr)){
#if GPIO_ENABLE
    if(state){
      gpio_cmd_output(PinLedBlu, 1);
    }else{
      gpio_cmd_output(PinLedBlu, 0);
    }
#endif
    sprintf(resp_body, "{\"blue\":\"%d\"}", state);
  }

#if DEBUG_LVL >= 1
  printf("%s\n", resp_body);
#endif

  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/wifi?val=1
 * returns { param : "value"}
 */
int callback_wifi_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char resp_body[256];
  char val[24];

  sprintf(val, "%s", u_map_get(request->map_url, "val"));

#if GPIO_ENABLE
  int state = atoi(val);
  if(state){
    gpio_cmd_output(PinWifiPwr, 1);
    sprintf(resp_body, "{\"wifi\":\"1\"}");
  }else{
    gpio_cmd_output(PinWifiPwr, 0);
    sprintf(resp_body, "{\"wifi\":\"0\"}");
  }
#endif

  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);
  
  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/mesh?val=1
 * returns { param : "value"}
 */
int callback_mesh_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char resp_body[256];
  char val[24];

  sprintf(val, "%s", u_map_get(request->map_url, "val"));

#if GPIO_ENABLE
  int state = atoi(val);
  if(state){
    gpio_cmd_output(PinMeshPwr, 1);
    sprintf(resp_body, "{\"mesh\":\"1\"}");
  }else{
    gpio_cmd_output(PinMeshPwr, 0);
    sprintf(resp_body, "{\"mesh\":\"0\"}");
  }
#endif

  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);

  return U_CALLBACK_CONTINUE;
}

/**
 * Read config param from config.json file
 * URL format:   http://localhost:8080/adc?chan=vin
 * returns { param : "value"}
 */
int callback_adc_control (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char resp_body[256];
  char chan[24];
  int adc = 0;

  sprintf(chan, "%s", u_map_get(request->map_url, "chan"));

  if(!strcmp("model",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_MODEL);
#endif
    sprintf(resp_body, "{\"model\":\"%d\"}", adc);
  }else if(!strcmp("hw",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_VERSION);
#endif
    sprintf(resp_body, "{\"hw\":\"%d\"}", adc);
  }else if(!strcmp("vin",chan)){
#if ADC_ENABLE
    adc = adc_read_raw(ADC_CHAN_VIN);
#endif
    sprintf(resp_body, "{\"vin\":\"%d\"}", adc);
  }else if(!strcmp("pb",chan)){
#if ADC_ENABLE
    adc = (int)gpio_get_input(PinBtn));
#endif
    sprintf(resp_body, "{\"pb\":\"%d\"}", adc);
  }

  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);
  
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

  char resp_body[256];
  sprintf(resp_body, "{\"%s\":\"%s\"}", param, value);
  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);
  
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

  char resp_body[256];
  sprintf(resp_body, "{\"%s\":\"%s\"}", param, value);
  cJSON* resp_json = cJSON_Parse(resp_body);
  set_cjson_body_response(response, 200, resp_json);

  return U_CALLBACK_CONTINUE;
}

/**
 * info page
 */
static int callback_info_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "info.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * control page
 */
static int callback_control_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "control.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * config page
 */
static int callback_config_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "config.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * admin page
 */
static int callback_admin_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "admin.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * neighbors page
 */
static int callback_neighbors_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "neighbors.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * sensors page
 */
static int callback_sensors_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "sensors.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * logging page
 */
static int callback_logging_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "logging.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * login page
 */
static int callback_login_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "login.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * logout page
 */
static int callback_logout_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "logout.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  ulfius_set_string_body_response(response, 200, buf);
  return U_CALLBACK_CONTINUE;
}

/**
 * test page
 */
static int callback_test_page (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char buf[PAGE_SIZE] = {0};
  char path[] = STATIC_PATH;
  char filename[] = "test.html";
  strcat(path, filename);
#if DEBUG_LVL >= 1
  printf("HTML: %s\n", path);
#endif

  FILE* fp = fopen((void *)path, "r");
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
  
  y_init_logs("webui_app", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Starting WebUI code ");
  
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
    
    // Start the framework
    if (ulfius_start_framework(&instance) == U_OK) {
      printf("Start WebUI at http://localhost:%u/info\n", instance.port);
      #if GPIO_ENABLE
      gpio_init();
      #endif
      
      // Wait for the user to press <enter> on the console to quit the application
      while(1);
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

