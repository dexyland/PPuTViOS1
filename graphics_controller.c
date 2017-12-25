#include "graphics_controller.h"
#include <directfb.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "pthread.h"


static void setTimerParams();
static void removeProgramNumber();
static void removeVolumeBar();
static void removeInfo();
static void* renderThread();
static void wipeScreen();


static IDirectFBImageProvider* provider;
static IDirectFBSurface* logoSurface = NULL;
static IDirectFBSurface* primary = NULL;
static IDirectFB* dfbInterface = NULL;
static DFBSurfaceDescription surfaceDesc;
static DFBFontDescription fontDesc;
static IDirectFBFont* fontInterface = NULL;
static int32_t screenWidth = 0;
static int32_t screenHeight = 0;
static int32_t logoHeight;
static int32_t logoWidth;

static uint8_t stopDrawing = 0;
static pthread_t gcThread;
static pthread_cond_t graphicsCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t graphicsMutex = PTHREAD_MUTEX_INITIALIZER;
static DrawComponents componentsToDraw;

static timer_t volumeTimer;
static struct itimerspec volumeTimerSpec;
static struct itimerspec volumeTimerSpecOld;
static struct sigevent volumeSignalEvent;
static timer_t infoTimer;
static struct itimerspec infoTimerSpec;
static struct itimerspec infoTimerSpecOld;
static struct sigevent infoSignalEvent;
static int32_t timerFlags = 0;

static int32_t numberOfKeys;
static int32_t keysToShow[3];


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

    /* create font */
    fontDesc.flags = DFDESC_HEIGHT;
    fontDesc.height = 40;

    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
    DFBCHECK(primary->SetFont(primary, fontInterface));

    /* clear the screen before drawing anything */
    wipeScreen();

    if (pthread_create(&gcThread, NULL, &renderThread, NULL))
    {
        printf("Error creating input event task!\n");
        return GC_THREAD_ERROR;
    }

    return GC_NO_ERROR;
}

GraphicsControllerError graphicsControllerDeinit()
{
    stopDrawing = 1;

    /* wait for render thread to finish */
    pthread_mutex_lock(&graphicsMutex);
    if (ETIMEDOUT == pthread_cond_wait(&graphicsCond, &graphicsMutex))
    {
        printf("\n%s : ERROR Lock timeout exceeded!\n", __FUNCTION__);
    }	
    pthread_mutex_unlock(&graphicsMutex);

    if (pthread_join(gcThread, NULL))
    {
        printf("\n%s : ERROR pthread_join fail!\n", __FUNCTION__);
        return GC_THREAD_ERROR;
    }

    timer_delete(volumeTimer);
    timer_delete(infoTimer);

    primary->Release(primary);
    dfbInterface->Release(dfbInterface);

    return GC_NO_ERROR;
}

void* renderThread()
{
    char tempString[20];

    while (!stopDrawing)
    {
        wipeScreen();

        if (componentsToDraw.showRadioLogo == true)
        {
            primary->SetColor(primary, 0x66, 0x00, 0x00, 0xFF);
            primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight);

            primary->SetColor(primary, 0xFF, 0xFF, 0x00, 0xEF);

            sprintf(tempString, "RADIO");

            DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/2 - 50, screenHeight/2, DSTF_LEFT));
		}

        if (componentsToDraw.showVolume)
        {
            switch (componentsToDraw.volume)
            {
                case 0:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_0.png", &provider));
                    break;
                case 1:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_1.png", &provider));
                    break;
                case 2:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_2.png", &provider));
                    break;
                case 3:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_3.png", &provider));
                    break;
                case 4:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_4.png", &provider));
                    break;
                case 5:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_5.png", &provider));
                    break;
                case 6:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_6.png", &provider));
                    break;
                case 7:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_7.png", &provider));
                    break;
                case 8:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_8.png", &provider));
                    break;
                case 9:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_9.png", &provider));
                    break;
                case 10:
                    DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "volume_10.png", &provider));
                    break;
            }

            DFBCHECK(provider->GetSurfaceDescription(provider, &surfaceDesc));
            DFBCHECK(dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &logoSurface));
            DFBCHECK(provider->RenderTo(provider, logoSurface, NULL));

            provider->Release(provider);

            DFBCHECK(logoSurface->GetSize(logoSurface, &logoWidth, &logoHeight));
            DFBCHECK(primary->Blit(primary, logoSurface, NULL, screenWidth - logoWidth - 100, 50));
        }

        if (componentsToDraw.showInfo)
        {
            primary->SetColor(primary, 0x00, 0x66, 0x99, 0xFF);
            primary->FillRectangle(primary, screenWidth/10 - 20, 3*screenHeight/4 - 20, 8*screenWidth/10 + 40, screenHeight/5 + 40);
            primary->SetColor(primary, 0xB3, 0xE6, 0xFF, 0xFF);
            primary->FillRectangle(primary, screenWidth/10, 3*screenHeight/4, 8*screenWidth/10, screenHeight/5);

            DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xFF));

            sprintf(tempString, "Program number : %d", componentsToDraw.programNumber);

            DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/9, 3*screenHeight/4 + 40, DSTF_LEFT));

            sprintf(tempString, "Video PID : %d", componentsToDraw.videoPidToDraw);

            DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/9, 3*screenHeight/4 + 80, DSTF_LEFT));

            sprintf(tempString, "Audio PID : %d", componentsToDraw.audioPidToDraw);

            DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/9, 3*screenHeight/4 + 120, DSTF_LEFT));

            if (componentsToDraw.hoursToDraw == 30)
            {
                sprintf(tempString, "Time not available");
            }
            else
            {
                sprintf(tempString, "%.2d:%.2d", componentsToDraw.hoursToDraw, componentsToDraw.minutesToDraw);
            }

            DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/9, screenHeight - 60, DSTF_LEFT));

            if (componentsToDraw.teletext == -1)
            {
                primary->SetColor(primary, 0xFF, 0x00, 0x00, 0xFF);
            }
            else
            {
                primary->SetColor(primary, 0x00, 0xFF, 0x00, 0xFF);
            }

            sprintf(tempString, "teletext");

            DFBCHECK(primary->DrawString(primary, tempString, -1, 7*screenWidth/9 + 40, 3*screenHeight/4 + 40, DSTF_LEFT));
        }

        if (componentsToDraw.showChannelDial)
        {
            primary->SetColor(primary, 0x00, 0x00, 0x00, 0xFF);
            primary->FillRectangle(primary, screenWidth/2 - 110 , screenHeight/2 - 210, 220, 70);

            primary->SetColor(primary, 0xFF, 0xFF, 0xFF, 0xFF);
            primary->FillRectangle(primary, screenWidth/2 - 100, screenHeight/2 - 200, 200, 50);

            primary->SetColor(primary, 0x00, 0x00, 0x00, 0xEF);

            if (numberOfKeys == 1)
            {
                sprintf(tempString, "%d", keysToShow[0]);
                DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/2 - 15, screenHeight/2 - 160, DSTF_LEFT));
            }
            else if (numberOfKeys == 2)
            {
                sprintf(tempString, "%d%d", keysToShow[0], keysToShow[1]);
                DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/2 - 25, screenHeight/2 - 160, DSTF_LEFT));
            }
            else if (numberOfKeys == 3)
            {
                sprintf(tempString, "%d%d%d", keysToShow[0], keysToShow[1], keysToShow[2]);
                DFBCHECK(primary->DrawString(primary, tempString, -1, screenWidth/2 - 35, screenHeight/2 - 160, DSTF_LEFT));
            }
        }

        DFBCHECK(primary->Flip(primary, NULL, 0));
    }

    pthread_mutex_lock(&graphicsMutex);
    pthread_cond_signal(&graphicsCond);
    pthread_mutex_unlock(&graphicsMutex);
}

void wipeScreen()
{
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
}

void drawVolumeBar(uint8_t volumeValue)
{
    componentsToDraw.volume = volumeValue;

    timer_settime(volumeTimer, timerFlags, &volumeTimerSpec, &volumeTimerSpecOld);
    componentsToDraw.showVolume = true;
}

void drawInfoRect(uint8_t hours, uint8_t minutes, int16_t audioPid, int16_t videoPid, uint8_t programNumber, int8_t teletext)
{
    timer_settime(infoTimer, timerFlags, &infoTimerSpec, &infoTimerSpecOld);

    componentsToDraw.audioPidToDraw = audioPid;
    componentsToDraw.videoPidToDraw = videoPid;
    componentsToDraw.hoursToDraw = hours;
    componentsToDraw.minutesToDraw = minutes;
    componentsToDraw.programNumber = programNumber;
    componentsToDraw.teletext = teletext;

    componentsToDraw.showInfo = true;
}

void setTimerParams()
{
    /* Settings for volume bar timer */
    volumeSignalEvent.sigev_notify = SIGEV_THREAD;
    volumeSignalEvent.sigev_notify_function = removeVolumeBar;
    volumeSignalEvent.sigev_value.sival_ptr = NULL;
    volumeSignalEvent.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &volumeSignalEvent, &volumeTimer);

    memset(&volumeTimerSpec, 0, sizeof(volumeTimerSpec));
    volumeTimerSpec.it_value.tv_sec = 3;
    volumeTimerSpec.it_value.tv_nsec = 0;

    /* Settings for info bar timer */
    infoSignalEvent.sigev_notify = SIGEV_THREAD;
    infoSignalEvent.sigev_notify_function = removeInfo;
    infoSignalEvent.sigev_value.sival_ptr = NULL;
    infoSignalEvent.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &infoSignalEvent, &infoTimer);

    memset(&infoTimerSpec, 0, sizeof(infoTimerSpec));
    infoTimerSpec.it_value.tv_sec = 3;
    infoTimerSpec.it_value.tv_nsec = 0;
}

void channelDial(uint8_t keysPressed, uint8_t keys[])
{
    numberOfKeys = keysPressed;
    keysToShow[0] = keys[0];
    keysToShow[1] = keys[1];
    keysToShow[2] = keys[2];
    componentsToDraw.showChannelDial = true;
}

void removeVolumeBar()
{
    componentsToDraw.showVolume = false;
}

void removeInfo()
{
    componentsToDraw.showInfo = false;
}

void removeChannelDial()
{
    componentsToDraw.showChannelDial = false;
}

void setRadioLogo()
{
    componentsToDraw.showRadioLogo = true;
}

void removeRadioLogo()
{
    componentsToDraw.showRadioLogo = false;
}
