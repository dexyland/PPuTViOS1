#include <directfb.h>

static IDirectFBSurface* primary = NULL;
static IdirectFB* dfbInterface = NULL;
static DFBSurfaceDescription surfaceDesc;
static int32_t screenWidth = 0;
static int32_t screenHeight = 0;
static uint8_t stopDrawing = 0;
static pthread_t gcThread;

static void renderThread();
static GraphicsControllerError wipeScreen();

GraphicsControllerError graphicsControllerInit()
{
	/* initialize DirectFB */
	if (DirectFBInit(0, NULL))
	{
		return GC_ERROR:
	}

	/* fetch the DirectFB interface */
	if (DirectFBCreate(&dfbInterface))
	{
		return GC_ERROR;
	}

	/* tell the DirectFB to take the full screen for this application */
	if (dfbInterface->SetCooperativeLevel(dfbInterface, DFSCL_FULLSCREEN))
	{
		return GC_ERROR;
	}

    /* create primary surface with double buffering enabled */
	surfaceDesc.flags = DSDESC_CAPS;
	surfaceDesc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	if (dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &primary))
	{
		return GC_ERROR;
	}    
    
    /* fetch the screen size */
    if (primary->GetSize(primary, &screenWidth, &screenHeight))
	{
		return GC_ERROR;
	}

	/* clear the screen before drawing anything */
	wipeScreen();

	if (pthread_create(&scThread, NULL, &renderThread, NULL))
    {
        printf("Error creating input event task!\n");
        return GC_THREAD_ERROR;
    }


	return GC_NO_ERROR;
}

void renderThread()
{
	while (!stopDrawing)
	{
		
	}
}

GraphicsControllerError wipeScreen()
{
	stopDrawing = 1;

	if (pthread_join(gcThread, NULL))
    {
        printf("\n%s : ERROR pthread_join fail!\n", __FUNCTION__);
        return GC_THREAD_ERROR;
    }

	/* clear the screen (draw black full screen rectangle) */
    if (primary->SetColor(primary, 0x00, 0x00 ,0x00, 0xff))
	{
		return GC_ERROR;
	}

	if (primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight))
	{
		return GC_ERROR;
	}

	return GC_NO_ERROR;
}
