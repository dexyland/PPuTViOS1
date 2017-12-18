#ifndef __GRAPHICS_CONTROLLER_H__
#define __GRAPHICS_CONTROLLER_H__

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
	bool showProgramNumer;
	bool showVolume;
	bool showInfo;
	int32_t programNumer;
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

#endif /* __GRAPHICS_CONTROLLER_H__ */
