#define TEST_MODE

#define RS485
#define RS485_TX
//#define RS485_RX
// RS485 TX mode for Nano
// RS485 RX mode for MEGA2560 testing 

#ifdef RS485_RX
#define RF24_CHANNEL_1
#endif
#ifdef RS485_TX
#define RF24_CHANNEL_2
#endif
//#define RF24_CHANNEL_3

#define VERBOSE_DEBUG_DEFAULT
//#define VERBOSE_DEBUG_VAR
//#define VERBOSE_DEBUG_RX
#define VERBOSE_DEBUG_DISTANCE_1
#define VERBOSE_DEBUG_DISTANCE_2
#define VERBOSE_DEBUG_TANK
#define VERBOSE_DEBUG_TANK_STATE    

#define DEFAULT_TANK_HEIGHT       1000
#define DEFAULT_TANK_FULL         700
#define DEFAULT_TANK_EMPTY        600

#define LCD_1602
#define LCD_NOKIA5110

#define LCD1602_POS_1     6
#define LCD5110_POS_1     4
