#include "graphics_controller.h"
#include <directfb.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>


static IDirectFBSurface* primary = NULL;
static IDirectFB* dfbInterface = NULL;
static DFBSurfaceDescription surfaceDesc;
static int32_t screenWidth = 0;
static int32_t screenHeight = 0;
static uint8_t stopDrawing = 0;

static pthread_t gcThread;
static DrawComponents componentsToDraw;

static timer_t programNumberTimer;
static struct itimerspec programTimerSpec;
static struct itimerspec programTimerSpecOld;
static struct sigevent programSignalEvent;

static timer_t volumeTimer;
static struct itimerspec volumeTimerSpec;
static struct itimerspec volumeTimerSpecOld;
static struct sigevent volumeSignalEvent;

static timer_t infoTimer;
static struct itimerspec infoTimerSpec;
static struct itimerspec infoTimerSpecOld;
static struct sigevent infoSignalEvent;

static int32_t timerFlags = 0;

static void removeProgramNumber();
static void removeVolumeBar();
static void removeInfo();
static void renderThread();
static void setTimerParams();
static void wipeScreen();


/* helper macro for error checking */
#define DFBCHECK(x...)                                      \
{                                                           \
DFBResult err = x;                                          \
                                                            \
if (err != DFB_OK)                                          \
  {                                                         \
    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );  \
    DirectFBErrorFatal( #x, err );                          \
  }                                                         \
}


GraphicsControllerError graphicsControllerInit()
{
	/* initialize DirectFB */
	if (DirectFBInit(0, NULL))
	{
		return GC_ERROR;
	}

	setTimerParams();

	componentsToDraw.showProgramNumber = false;
	componentsToDraw.showVolume = false;
	componentsToDraw.showInfo = false;
	componentsToDraw.programNumber = 0;
	componentsToDraw.volume = 0;

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

	if (pthread_create(&gcThread, NULL, &renderThread, NULL))
    {
        printf("Error creating input event task!\n");
        return GC_THREAD_ERROR;
    }
	else
	{
		printf("Render thread created!\n");
	}


	return GC_NO_ERROR;
}

GraphicsControllerError graphicsControllerDeinit()
{
	stopDrawing = 1;

	if (pthread_join(gcThread, NULL))
    {
        printf("\n%s : ERROR pthread_join fail!\n", __FUNCTION__);
        return GC_THREAD_ERROR;
    }

	printf("GCTHREAD joined!\n");

	timer_delete(programNumberTimer);
	timer_delete(volumeTimer);
	timer_delete(infoTimer);

	primary->Release(primary);
	dfbInterface->Release(dfbInterface);

	return GC_NO_ERROR;
}

void renderThread()
{
	while (!stopDrawing)
	{
		wipeScreen();
		
		if (componentsToDraw.showProgramNumber)
		{
			primary->SetColor(primary, 0xFF, 0x00, 0xFF, 0xFF);
    		primary->FillRectangle(primary, screenWidth/10, screenHeight/6, 3*screenWidth/10, 2*screenHeight/6);
		}

		if (componentsToDraw.showVolume)
		{
			primary->SetColor(primary, 0x00, 0xFF, 0x00, 0xFF);
    		primary->FillRectangle(primary, 7*screenWidth/10, screenHeight/6, 9*screenWidth/10, 2*screenHeight/6);
		}

		if (componentsToDraw.showInfo)
		{
			primary->SetColor(primary, 0xFF, 0x00, 0x00, 0xFF);
    		primary->FillRectangle(primary, screenWidth/10, 3*screenHeight/4, 8*screenWidth/10, 17*screenHeight/20);
		}

		DFBCHECK(primary->Flip(primary, NULL, 0));
	}
}

void wipeScreen()
{
    /* clear screen */
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    
    /* update screen */
   	//DFBCHECK(primary->Flip(primary, NULL, 0));
}

void drawProgramNumber()
{

	timer_settime(programNumberTimer, timerFlags, &programTimerSpec, &programTimerSpecOld);
	componentsToDraw.showProgramNumber = true;
}

void drawVolumeBar()
{

	timer_settime(volumeTimer, timerFlags, &volumeTimerSpec, &volumeTimerSpecOld);
	componentsToDraw.showVolume = true;
}

void drawInfoRect()
{
	timer_settime(infoTimer, timerFlags, &infoTimerSpec, &infoTimerSpecOld);
	componentsToDraw.showInfo = true;
}

void setTimerParams()
{
	/* Setting timer for program number */
	programSignalEvent.sigev_notify = SIGEV_THREAD;
	programSignalEvent.sigev_notify_function = removeProgramNumber;
	programSignalEvent.sigev_value.sival_ptr = NULL;
	programSignalEvent.sigev_notify_attributes = NULL;
	timer_create(CLOCK_REALTIME, &programSignalEvent, &programNumberTimer);

	/*  */
	memset(&programTimerSpec, 0, sizeof(programTimerSpec));
	programTimerSpec.it_value.tv_sec = 4;
	programTimerSpec.it_value.tv_nsec = 0;

	/* Setting timer for volume bar */
	volumeSignalEvent.sigev_notify = SIGEV_THREAD;
	volumeSignalEvent.sigev_notify_function = removeVolumeBar;
	volumeSignalEvent.sigev_value.sival_ptr = NULL;
	volumeSignalEvent.sigev_notify_attributes = NULL;
	timer_create(CLOCK_REALTIME, &volumeSignalEvent, &volumeTimer);

    /* */
	memset(&volumeTimerSpec, 0, sizeof(volumeTimerSpec));
	volumeTimerSpec.it_value.tv_sec = 3;
	volumeTimerSpec.it_value.tv_nsec = 0;

	/* Setting timer for info bar */
	infoSignalEvent.sigev_notify = SIGEV_THREAD;
	infoSignalEvent.sigev_notify_function = removeInfo;
	infoSignalEvent.sigev_value.sival_ptr = NULL;
	infoSignalEvent.sigev_notify_attributes = NULL;
	timer_create(CLOCK_REALTIME, &infoSignalEvent, &infoTimer);

	/* */
	memset(&infoTimerSpec, 0, sizeof(infoTimerSpec));
	infoTimerSpec.it_value.tv_sec = 3;
	infoTimerSpec.it_value.tv_nsec = 0;
}

void removeProgramNumber()
{
	componentsToDraw.showProgramNumber = false;
}

void removeVolumeBar()
{
	componentsToDraw.showVolume = false;
}

void removeInfo()
{
	componentsToDraw.showInfo = false;
}

