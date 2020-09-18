/*
 * Logger.h
 *
 *  Created on: Sep 18, 2020
 *      Author: brain
 */

#ifndef LOGGER_H
#define LOGGER_H

#define LOG_NONE      0
#define LOG_ERROR     1
#define LOG_ERR       1
#define LOG_WARN      2
#define LOG_INFO      3
#define LOG_DEBUG     4
#define LOG_VERBOSE   5

#ifndef LOGGER
#define LOGGER        SERIAL_PORT_MONITOR
#endif

#define LOG_BUFSIZE     100

#if defined LOG_LEVEL && LOG_LEVEL >= LOG_ERROR
void LOG(uint8_t lvl, const char *fmt, ...);
void LOG_P(uint8_t lvl, const char *fmt, ...);
#endif

#if defined LOG_LEVEL && LOG_LEVEL >= LOG_ERROR
  #define LOGE(fmt, ...)      LOG(LOG_ERROR, fmt, ##__VA_ARGS__)
  #define LOGE_P(fmt, ...)    LOG_P(LOG_ERROR, PSTR(fmt), ##__VA_ARGS__)
#else
  #define LOGE(...)           do {} while(0)
  #define LOGE_P(...)         do {} while(0)
#endif
#if defined LOG_LEVEL && LOG_LEVEL >= LOG_WARN
  #define LOGW(fmt, ...)      LOG(LOG_WARN, fmt, ##__VA_ARGS__)
  #define LOGW_P(fmt, ...)    LOG_P(LOG_WARN, PSTR(fmt), ##__VA_ARGS__)
#else
  #define LOGW(...)           do {} while(0)
  #define LOGW_P(...)         do {} while(0)
#endif
#if defined LOG_LEVEL && LOG_LEVEL >= LOG_INFO
  #define LOGI(fmt, ...)      LOG(LOG_INFO, fmt, ##__VA_ARGS__)
  #define LOGI_P(fmt, ...)    LOG_P(LOG_INFO, PSTR(fmt), ##__VA_ARGS__)
#else
  #define LOGI(...)           do {} while(0)
  #define LOGI_P(...)         do {} while(0)
#endif
#if defined LOG_LEVEL && LOG_LEVEL >= LOG_DEBUG
  #define LOGD(fmt, ...)      LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
  #define LOGD_P(fmt, ...)    LOG_P(LOG_DEBUG, PSTR(fmt), ##__VA_ARGS__)
#else
  #define LOGD(...)           do {} while(0)
  #define LOGD_P(...)         do {} while(0)
#endif
#if defined LOG_LEVEL && LOG_LEVEL >= LOG_VERBOSE
  #define LOGV(fmt, ...)      LOG(LOG_VERBOSE, fmt, ##__VA_ARGS__)
  #define LOGV_P(fmt, ...)    LOG_P(LOG_VERBOSE, PSTR(fmt), ##__VA_ARGS__)
#else
  #define LOGV(...)           do {} while(0)
  #define LOGV_P(...)         do {} while(0)
#endif


#endif /* LOGGER_H */
