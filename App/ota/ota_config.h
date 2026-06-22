#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_WIFI_SSID                 "www"
#define OTA_WIFI_PASSWORD             "www123456"

#define ONENET_USER_ID                "498339"
#define ONENET_PRODUCT_ID             "S9U9FY9ZdS"
#define ONENET_DEVICE_NAME            "Bracelet"

#define ONENET_MQTT_HOST              "mqtts.heclouds.com"
#define ONENET_MQTT_PORT              1883U
#define ONENET_MQTT_CLIENT_ID         ONENET_DEVICE_NAME
#define ONENET_MQTT_USERNAME          ONENET_PRODUCT_ID
#define ONENET_MQTT_PASSWORD          "version=2018-10-31&res=products%2FS9U9FY9ZdS%2Fdevices%2FBracelet&et=1805863774&method=md5&sign=h%2F8qRCOICNVzmUC0cTq5Bg%3D%3D"

#define OTA_HTTP_HOST                 "iot-api.heclouds.com"
#define OTA_HTTP_PORT                 80U
#define OTA_AUTHORIZATION             "version=2022-05-01&res=userid%2F498339&et=1813592379&method=sha1&sign=zJumjVG6TF9IFVmhszVgKyC1qo4%3D"
#define OTA_CURRENT_VERSION           "V1.0"
#define OTA_UPGRADE_TYPE              2U
#define OTA_DOWNLOAD_CHUNK_SIZE       256U

#ifdef __cplusplus
}
#endif

#endif /* OTA_CONFIG_H */
