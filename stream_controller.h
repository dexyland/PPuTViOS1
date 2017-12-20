#ifndef __STREAM_CONTROLLER_H__
#define __STREAM_CONTROLLER_H__

#include <stdio.h>
#include "tables.h"
#include "tdp_api.h"
#include "tables.h"
#include "pthread.h"
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#define DESIRED_FREQUENCY 754000000	        /* Tune frequency in Hz */
#define BANDWIDTH 8    				        /* Bandwidth in Mhz */
#define LINE_LENGTH 100						/* Max line length in config file */

/**
 * @brief Structure that defines stream controller error
 */
typedef enum _StreamControllerError
{
    SC_NO_ERROR = 0,
    SC_ERROR,
    SC_THREAD_ERROR
}StreamControllerError;

/**
 * @brief Structure that defines channel info
 */
typedef struct _ChannelInfo
{
    int16_t programNumber;
    int16_t audioPid;
    int16_t videoPid;
}ChannelInfo;

/**
 * @brief Structure that holds initial info
 */
typedef struct _InitialInfo
{
	uint32_t tuneFrequency;
	uint32_t tuneBandwidth;
	uint32_t programNumber;
	t_Module tuneModule;
}InitialInfo;

/**
 * @brief Structure that holds time when TOT table was received
 */
typedef struct _TimeStructure
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	time_t timeStampSeconds;
}TimeStructure;

/**
 * @brief Time callback
 */
typedef void(*TimeCallback)(TimeStructure* timeStructure);

/*
 * @brief Registers time callback
 *
 * @param  [in] time callback - pointer to time callback function
 * @return Stream controller error code
 */
StreamControllerError registerTimeCallback(TimeCallback timeCallback);

/**
 * @brief Initializes stream controller module
 *
 * @return stream controller error code
 */
StreamControllerError streamControllerInit();

/**
 * @brief Deinitializes stream controller module
 *
 * @return stream controller error code
 */
StreamControllerError streamControllerDeinit();

/**
 * @brief Channel up
 *
 * @return stream controller error
 */
StreamControllerError channelUp();

/**
 * @brief Channel down
 *
 * @return stream controller error
 */
StreamControllerError channelDown();

/**
 * @brief Returns current channel info
 *
 * @param [out] channelInfo - channel info structure with current channel info
 * @return stream controller error code
 */
StreamControllerError getChannelInfo(ChannelInfo* channelInfo);

/**
 * @brief Loads config.ini file holding initial info
 *
 * @return stream conotroller error code
 */
StreamControllerError loadInitialInfo();

/**
 * @brief 
 */
void changeChannelKey(int32_t channelNumber);

#endif /* __STREAM_CONTROLLER_H__ */
