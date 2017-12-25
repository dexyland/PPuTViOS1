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
 * @brief Deinitializes graphics controller module
 *
 * @return graphics controller error code
 */
void drawVolumeBar(uint8_t volumeValue);

/**
 * @brief Deinitializes graphics controller module
 *
 * @return graphics controller error code
 */
void drawInfoRect(uint8_t hours, uint8_t minutes, int16_t audioPid, int16_t videoPid, uint8_t programNumber, int8_t teletext);

void channelDial(uint8_t keysPressed, uint8_t keys[]);

void setRadioLogo();

void removeRadioLogo();

void removeChannelDial();

#endif /* __GRAPHICS_CONTROLLER_H__ */
