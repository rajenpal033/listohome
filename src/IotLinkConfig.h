#ifndef __IOTLINK_CONFIG_H__
#define __IOTLINK_CONFIG_H__


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// Version Configuration
#define IOTLINK_VERSION_MAJOR     1
#define IOTLINK_VERSION_MINOR     1
#define IOTLINK_VERSION_REVISION  1
#define IOTLINK_VERSION STR(IOTLINK_VERSION_MAJOR) "." STR(IOTLINK_VERSION_MINOR) "." STR(IOTLINK_VERSION_REVISION)

// Server Configuration
#define IOTLINK_SERVER_URL "iotlink.io"
#define IOTLINK_SERVER_PORT 3100


// WebSocket Configuration
#define WEBSOCKET_PING_INTERVAL 300000
#define WEBSOCKET_PING_TIMEOUT 10000
#define WEBSOCKET_RETRY_COUNT 2

// LeakyBucket Configuration
#define BUCKET_SIZE 10
#define DROP_OUT_TIME 60000
#define DROP_IN_TIME 1000u

#endif
