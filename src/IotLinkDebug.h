#ifndef __IOTLINKDEBUG_H__
#define __IOTLINKDEBUG_H__

#ifndef NODEBUG_IOTLINK
#ifdef DEBUG_ESP_PORT
#define DEBUG_IOTLINK(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#else
//#define DEBUG_WEBSOCKETS(...) os_printf( __VA_ARGS__ )
#endif
#endif


#ifndef DEBUG_IOTLINK
#define DEBUG_IOTLINK(...)
#define NODEBUG_IOTLINK
#endif


#endif
