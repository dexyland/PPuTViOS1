#include "remote_controller.h"
#include "stream_controller.h"
#include <signal.h>

static inline void textColor(int32_t attr, int32_t fg, int32_t bg)
{
   char command[13];

   /* command is the control command to the terminal */
   sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
   printf("%s", command);
}

/* macro function for error checking */
#define ERRORCHECK(x)                                                       \
{                                                                           \
if (x != 0)                                                                 \
 {                                                                          \
    textColor(1,1,0);                                                       \
    printf(" Error!\n File: %s \t Line: <%d>\n", __FILE__, __LINE__);       \
    textColor(0,7,0);                                                       \
    return -1;                                                              \
 }                                                                          \
}
void inputChannelNumber(uint16_t key);
void changeChannel();

static void remoteControllerCallback(uint16_t code, uint16_t type, uint32_t value);
static pthread_cond_t deinitCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t deinitMutex = PTHREAD_MUTEX_INITIALIZER;
static ChannelInfo channelInfo;
static int32_t keysPressed = 0;
static int32_t keyOne = 0;
static int32_t keyTwo = 0;
static int32_t keyThree = 0;

static timer_t keyTimer;
static struct itimerspec timerSpec;
static struct itimerspec timerSpecOld;
static int32_t timerFlags = 0;

/*struct sigevent {
	int sigev_notify;
	int sigev_signo;
	union sigval sigev_value;
	void (*sigev_notify_function) (union sigval);
	void *sigev_notify_attributes;
	pid_t sigev_notify_thread_id;
};

union sigval {
	int sival_int;
	void *sival_ptf;
};
*/
int main()
{
	/*struct sigevent signalEvent;
	signalEvent.sigev_notify = SIGEV_THREAD;
	signalEvent.sigev_notify_function = changeChannel;
	signalEvent.sigev_value.sival_ptr = NULL;
	signalEvent.sigev_notify_attributes = NULL;
	timer_create(CLOCK_REALTIME, &signalEvent, &keyTimer);

	printf("Napravio timer!\n");

	memset(&timerSpec, 0, sizeof(timerSpec));
	timerSpec.it_value.tv_sec = 3;
	timerSpec.it_value.tv_nsec = 0;
*/
	/* load initial info from config.ini file */
	if (loadInitialInfo())
	{
		printf("Initial info required!\n");
		return -1;
	}

    /* initialize remote controller module */
    ERRORCHECK(remoteControllerInit());
    
    /* register remote controller callback */
    ERRORCHECK(registerRemoteControllerCallback(remoteControllerCallback));
    
    /* initialize stream controller module */
    ERRORCHECK(streamControllerInit());

    /* wait for a EXIT remote controller key press event */
    pthread_mutex_lock(&deinitMutex);
	if (ETIMEDOUT == pthread_cond_wait(&deinitCond, &deinitMutex))
	{
		printf("\n%s : ERROR Lock timeout exceeded!\n", __FUNCTION__);
	}
	pthread_mutex_unlock(&deinitMutex);

	//timer_delete(keyTimer);
    
    /* unregister remote controller callback */
    ERRORCHECK(unregisterRemoteControllerCallback(remoteControllerCallback));

    /* deinitialize remote controller module */
    ERRORCHECK(remoteControllerDeinit());

    /* deinitialize stream controller module */
    ERRORCHECK(streamControllerDeinit());
  
    return 0;
}

void remoteControllerCallback(uint16_t code, uint16_t type, uint32_t value)
{
    switch(code)
	{
		case KEYCODE_INFO:
            printf("\nInfo pressed\n");          
            if (getChannelInfo(&channelInfo) == SC_NO_ERROR)
            {
                printf("\n********************* Channel info *********************\n");
                printf("Program number: %d\n", channelInfo.programNumber);
                printf("Audio pid: %d\n", channelInfo.audioPid);
                printf("Video pid: %d\n", channelInfo.videoPid);
                printf("**********************************************************\n");
            }
			break;
		case KEYCODE_P_PLUS:
			printf("\nCH+ pressed\n");
            channelUp();
			break;
		case KEYCODE_P_MINUS:
		    printf("\nCH- pressed\n");
            channelDown();
			break;
		case KEYCODE_EXIT:
			printf("\nExit pressed\n");
            pthread_mutex_lock(&deinitMutex);
		    pthread_cond_signal(&deinitCond);
		    pthread_mutex_unlock(&deinitMutex);
			break;
		case KEYCODE_1:
			printf("\nKey 1 pressed\n");
			inputChannelNumber(1);
			break;
		case KEYCODE_2:
			printf("\nKey 2 pressed\n");
			inputChannelNumber(2);
			break;
		case KEYCODE_3:
			printf("\nKey 3 pressed\n");
			inputChannelNumber(3);
			break;
		case KEYCODE_4:
			printf("\nKey 4 pressed\n");
			inputChannelNumber(4);
			break;
		case KEYCODE_5:
			printf("\nKey 5 pressed\n");
			inputChannelNumber(5);
			break;
		case KEYCODE_6:
			printf("\nKey 6 pressed\n");
			inputChannelNumber(6);
			break;
		case KEYCODE_7:
			printf("\nKey 7 pressed\n");
			inputChannelNumber(7);
			break;
		case KEYCODE_8:
			printf("\nKey 8 pressed\n");
			inputChannelNumber(8);
			break;
		case KEYCODE_9:
			printf("\nKey 9 pressed\n");
			inputChannelNumber(9);
			break;
		case KEYCODE_0:
			printf("\nKey 0 pressed\n");
			inputChannelNumber(0);
			break;
		default:
			printf("\nPress P+, P-, info or exit! \n\n");
	}
}

void inputChannelNumber(uint16_t key)
{
	//timer_settime(keyTimer, timerFlags, &timerSpec, &timerSpecOld);

	if (keysPressed == 0)
	{
		keyOne = key;
		keysPressed++;
	}
	else if (keysPressed == 1)
	{
		keyTwo = key;
		keysPressed++;
	}
	else if (keysPressed == 2)
	{
		keyThree = key;
		keysPressed++;
		changeChannel();
	}

	printf("\nKeyOne: %d\n", keyOne);
	printf("KeyTwo: %d\n", keyTwo);
	printf("KeyThree: %d\n", keyThree);
	printf("Keypressed: %d\n", keysPressed);
}

void changeChannel()
{
	int32_t channel;

	if (keysPressed == 1)
	{
		channel = keyOne;
	}
	else if (keysPressed == 2)
	{
		channel = 10*keyOne + keyTwo;
	}
	else if (keysPressed == 3)
	{
		channel = 100*keyOne + 10*keyTwo + keyThree;
	}

	changeChannelKey(channel);

	keysPressed = 0;
}


