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
	bool showProgramNumber;
	bool showVolume;
	bool showInfo;
	int32_t programNumber;
	int32_t volume;
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
void drawProgramNumber();

/**
 * @brief Deinitializes graphics controller module
 *
 * @return graphics controller error code
 */
void drawVolumeBar();

/**
 * @brief Deinitializes graphics controller module
 *
 * @return graphics controller error code
 */
void drawInfoRect();

#endif /* __GRAPHICS_CONTROLLER_H__ */
