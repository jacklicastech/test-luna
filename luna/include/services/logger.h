/* 
 * File:   logger.h
 * Author: Colin
 *
 * Created on December 14, 2015, 4:14 PM
 */

#ifndef LOGGER_H
#define	LOGGER_H

#ifdef	__cplusplus
extern "C" {
#endif

  #define LOG_LEVEL_SILENT 10
  #define LOG_LEVEL_ERROR   3
  #define LOG_LEVEL_WARN    2
  #define LOG_LEVEL_INFO    1
  #define LOG_LEVEL_DEBUG   0
  #define LOG_LEVEL_TRACE  -1
  #define LOG_LEVEL_INSEC  -2

  void LINFO(const char *fmt, ...);
  void LTRACE(const char *fmt, ...);
  void LINSEC(const char *fmt, ...);
  void LDEBUG(const char *fmt, ...);
  void LWARN(const char *fmt, ...);
  void LERROR(const char *fmt, ...);
  void LSETLEVEL(int level);
  int  LGETLEVEL();

  int init_logger_service(int log_level);
  void shutdown_logger_service(void);

#ifdef	__cplusplus
}
#endif

#endif	/* LOGGER_H */

