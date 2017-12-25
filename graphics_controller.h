#ifndef __GRAPHICS_CONTROLLER_H__
#define __GRAPHICS_CONTROLLER_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Structure that defines stream controller error
 */
typedef enum _GraphicsControllerError
{
    GC_NO_ERROR = 0,
    GC_ERROR,
    GC_THREAD_ERROR
}GraphicsControllerError;

/**
 * @brief Structure that holds draw components flags and values
 */
typedef struct _DrawComponents
{
    bool showVolume;
    bool showInfo;
    bool showChannelDial;
    bool showRadioLogo;
    int8_t teletext;
    uint8_t hoursToDraw;
    uint8_t minutesToDraw;
    uint8_t programNumber;
    uint8_t volume;
    int16_t audioPidToDraw;
    int16_t videoPidToDraw;
}DrawComponents;

/**
 * @brief Initializes graphics controller module
 *
 * @return graphics controller error code
 */
GraphicsControllerError graphicsControllerInit();

/**
 * @brief Deinitializes graphics controller module
 *
 * @return graphics controller error code
 */
GraphicsControllerError graphicsControllerDeinit();

/**
 * @brief Initiates drawing of volume logo
 *
 * @param [in] volumeValue - current volume value
 */
void drawVolumeBar(uint8_t volumeValue);

/**
 * @brief Initiates drawing of info bar
 *
 * @param [in] hours - current hours value
 * @param [in] minutes - current minutes value
 * @param [in] audioPid - current channel audio PID
 * @param [in] videoPid - current channel video PID
 * @param [in] programNumber - current program number
 * @param [in] teletext - teletext available or not (-1)
 */
void drawInfoRect(uint8_t hours, uint8_t minutes, int16_t audioPid, int16_t videoPid, uint8_t programNumber, int8_t teletext);

/**
 * @brief Initiates drawing of channel number dial rectangle
 *
 * @param [in] keysPressed - number of digits in channel number
 * @param [in] keys[] - array of digits in channel number
 */
void channelDial(uint8_t keysPressed, uint8_t keys[]);

/**
 * @brief Removes channel nubmer dial rectangle
 */
void removeChannelDial();

/**
 * @brief Paints filled rectangle over the screen
 */
void setRadioLogo();

/**
 * @brief Removes filled rectangle from the screen
 */
void removeRadioLogo();

#endif /* __GRAPHICS_CONTROLLER_H__ */
