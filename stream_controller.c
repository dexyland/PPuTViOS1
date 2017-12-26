#include "stream_controller.h"

#define LINE_LENGTH 100          /* Max line length in config file */


static StreamControllerError loadConfigFile(char* filename, InitialInfo* configInfo);
static void startChannel(int32_t channelNumber);
static void removeWhiteSpaces(char* string);
static void* streamControllerTask();
static void* parseTimeTables();
static int32_t sectionReceivedCallback(uint8_t *buffer);
static int32_t tunerStatusCallback(t_LockStatus status);


static PatTable *patTable;
static PmtTable *pmtTable;
static TdtTable *tdtTable;
static TotTable *totTable;

static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t initCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t demuxCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t demuxMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t scThread;

static uint32_t playerHandle = 0;
static uint32_t sourceHandle = 0;
static uint32_t streamHandleA = 0;
static uint32_t streamHandleV = 0;
static uint32_t filterHandle = 0;

static VolumeCallback volumeReportCallback = NULL;
static TimeCallback timeRecievedCallback = NULL;
static ProgramTypeCallback programType = NULL;

static bool timeTablesRecieved = false;
static bool changeChannel = false;
static bool isInitialized = false;

static uint32_t volumeConstant = 160400000;
static uint32_t currentVolume = 5;
static int16_t programNumber = 0;
static uint8_t threadExit = 0;

static ChannelInfo currentChannel;
static TimeStructure startTime;
static InitialInfo configFile;

static struct timespec lockStatusWaitTime;
static struct timeval now;


StreamControllerError streamControllerInit()
{
    if (pthread_create(&scThread, NULL, &streamControllerTask, NULL))
    {
        printf("Error creating input event task!\n");
        return SC_THREAD_ERROR;
    }

    return SC_NO_ERROR;
}

StreamControllerError streamControllerDeinit()
{
    if (!isInitialized)
    {
        pthread_mutex_lock(&initMutex);
        pthread_cond_wait(&initCond, &initMutex);
        pthread_mutex_unlock(&initMutex);
    }
    
    if (!isInitialized) 
    {
        printf("\n%s : ERROR streamControllerDeinit() fail, module is not initialized!\n", __FUNCTION__);
        return SC_ERROR;
    }
    
    threadExit = 1;
    if (pthread_join(scThread, NULL))
    {
        printf("\n%s : ERROR pthread_join fail!\n", __FUNCTION__);
        return SC_THREAD_ERROR;
    }
    
    /* free demux filter */  
    Demux_Free_Filter(playerHandle, filterHandle);

    /* remove audio stream */
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleA);
    
    /* remove video stream */
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleV);
    
    /* close player source */
    Player_Source_Close(playerHandle, sourceHandle);
    
    /* deinitialize player */
    Player_Deinit(playerHandle);
    
    /* deinitialize tuner device */
    Tuner_Deinit();
    
    /* free allocated memory */  
    free(patTable);
    free(pmtTable);
    free(tdtTable);
    free(totTable);

    /* set isInitialized flag */
    isInitialized = false;

    return SC_NO_ERROR;
}

StreamControllerError channelUp()
{   
    if (programNumber >= patTable->serviceInfoCount - 2)
    {
        programNumber = 0;
    } 
    else
    {
        programNumber++;
    }

    /* set flag to start current channel */
    changeChannel = true;

    return SC_NO_ERROR;
}

StreamControllerError channelDown()
{
    if (programNumber <= 0)
    {
        programNumber = patTable->serviceInfoCount - 2;
    } 
    else
    {
        programNumber--;
    }
   
    /* set flag to start current channel */
    changeChannel = true;

    return SC_NO_ERROR;
}

StreamControllerError getChannelInfo(ChannelInfo* channelInfo)
{
    if (channelInfo == NULL)
    {
        printf("\n Error wrong parameter\n", __FUNCTION__);
        return SC_ERROR;
    }
    
    channelInfo->programNumber = currentChannel.programNumber;
    channelInfo->audioPid = currentChannel.audioPid;
    channelInfo->videoPid = currentChannel.videoPid;
    channelInfo->teletext = currentChannel.teletext;
    
    return SC_NO_ERROR;
}

/* Sets filter to receive current channel PMT table
 * Parses current channel PMT table when it arrives
 * Creates streams with current channel audio and video pids
 */
void startChannel(int32_t channelNumber)
{
    /* free PAT table filter */
    Demux_Free_Filter(playerHandle, filterHandle);
    
    /* set demux filter for receive PMT table of program */
    if(Demux_Set_Filter(playerHandle, patTable->patServiceInfoArray[channelNumber + 1].pid, 0x02, &filterHandle))
    {
        printf("\n%s : ERROR Demux_Set_Filter() fail\n", __FUNCTION__);
        return;
    }
    
    /* wait for a PMT table to be parsed*/
    pthread_mutex_lock(&demuxMutex);
    if (ETIMEDOUT == pthread_cond_wait(&demuxCond, &demuxMutex))
    {
        printf("\n%s : ERROR Lock timeout exceeded!\n", __FUNCTION__);
        streamControllerDeinit();
    }
    pthread_mutex_unlock(&demuxMutex);
    
    /* get audio and video pids */
    int16_t audioPid = -1;
    int16_t videoPid = -1;
    int8_t teletext = -1;
    uint8_t i = 0;
    for (i = 0; i < pmtTable->elementaryInfoCount; i++)
    {
        if (((pmtTable->pmtElementaryInfoArray[i].streamType == 0x1) || (pmtTable->pmtElementaryInfoArray[i].streamType == 0x2) || (pmtTable->pmtElementaryInfoArray[i].streamType == 0x1b))
            && (videoPid == -1))
        {
            videoPid = pmtTable->pmtElementaryInfoArray[i].elementaryPid;
        } 
        else if (((pmtTable->pmtElementaryInfoArray[i].streamType == 0x3) || (pmtTable->pmtElementaryInfoArray[i].streamType == 0x4))
            && (audioPid == -1))
        {
            audioPid = pmtTable->pmtElementaryInfoArray[i].elementaryPid;
        }

        if (pmtTable->pmtElementaryInfoArray[i].elementaryPid == 0x56)
        {
            teletext = 1;
        }
    }

    if (videoPid != -1) 
    {
        /* remove previous video stream */
        if (streamHandleV != 0)
        {
            Player_Stream_Remove(playerHandle, sourceHandle, streamHandleV);
            streamHandleV = 0;
        }

        /* create video stream */
        if(Player_Stream_Create(playerHandle, sourceHandle, videoPid, VIDEO_TYPE_MPEG2, &streamHandleV))
        {
            printf("\n%s : ERROR Cannot create video stream\n", __FUNCTION__);
            streamControllerDeinit();
        }
    }

    if (audioPid != -1)
    {   
        /* remove previos audio stream */
        if (streamHandleA != 0)
        {
            Player_Stream_Remove(playerHandle, sourceHandle, streamHandleA);
            streamHandleA = 0;
        }

        /* create audio stream */
        if(Player_Stream_Create(playerHandle, sourceHandle, audioPid, AUDIO_TYPE_MPEG_AUDIO, &streamHandleA))
        {
            printf("\n%s : ERROR Cannot create audio stream\n", __FUNCTION__);
            streamControllerDeinit();
        }
    }
    
    /* store current channel info */
    currentChannel.programNumber = channelNumber + 1;
    currentChannel.audioPid = audioPid;
    currentChannel.videoPid = videoPid;
    currentChannel.teletext = teletext;

    programType(videoPid);

    if (timeTablesRecieved == false)
    {
        parseTimeTables();
    }
}

void* parseTimeTables()
{
    struct timeval tempTime;

    /* free previous table filter */
    Demux_Free_Filter(playerHandle, filterHandle);

    /* set demux filter for receive TDT table of program */
    if(Demux_Set_Filter(playerHandle, 0x0014, 0x70, &filterHandle))
    {
        printf("\n%s : ERROR Demux_Set_Filter() fail\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }

    /* wait for a TDT table to be parsed*/
    pthread_mutex_lock(&demuxMutex);
    if (ETIMEDOUT == pthread_cond_wait(&demuxCond, &demuxMutex))
    {
        printf("\n%s : ERROR Lock timeout exceeded!\n", __FUNCTION__);
        streamControllerDeinit();
    }    
    pthread_mutex_unlock(&demuxMutex);

    gettimeofday(&tempTime, NULL);

    /* free TDT table filter */
    Demux_Free_Filter(playerHandle, filterHandle);

    /* set demux filter for receive TOT table of program */
    if(Demux_Set_Filter(playerHandle, 0x0014, 0x73, &filterHandle))
    {
        printf("\n%s : ERROR Demux_Set_Filter() fail\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }

    /* wait for a TDT table to be parsed*/
    pthread_mutex_lock(&demuxMutex);
    if (ETIMEDOUT == pthread_cond_wait(&demuxCond, &demuxMutex))
    {
        printf("\n%s : ERROR Lock timeout exceeded!\n", __FUNCTION__);
        streamControllerDeinit();
    }
    pthread_mutex_unlock(&demuxMutex);

    uint8_t offsetHours = totTable->descriptors[0].ltoInfo[0].localTimeOffsetHours;
    uint8_t offsetMinutes = totTable->descriptors[0].ltoInfo[0].localTimeOffsetMinutes;

    startTime.hours = tdtTable->hours;
    startTime.minutes = tdtTable->minutes;
    startTime.seconds = tdtTable->seconds;
    startTime.timeStampSeconds = tempTime.tv_sec;

    if (totTable->descriptors[0].ltoInfo[0].localTimeOffsetPolarity == 0)
    {
        startTime.hours += offsetHours;
        startTime.minutes += offsetMinutes;

        if (startTime.hours > 23)
        {
            startTime.hours -= 24;
        }

        if (startTime.minutes > 59)
        {
            startTime.minutes -= 60;
        }
    }
    else if (totTable->descriptors[0].ltoInfo[0].localTimeOffsetPolarity == 1)
    {
        if (offsetHours > startTime.hours)
        {
            startTime.hours = 24 - offsetHours;
        }
        else
        {
            startTime.hours -= offsetHours;
        }

        if (offsetMinutes > startTime.minutes)
        {
            startTime.hours = 60 - offsetMinutes;
        }
        else
        {
            startTime.minutes -= offsetMinutes;
        }
    }

    timeRecievedCallback(&startTime);

    printf("Time tables parsed!\n");

    timeTablesRecieved = true;
}

void* streamControllerTask()
{
    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;

    /* allocate memory for PAT table section */
    patTable=(PatTable*)malloc(sizeof(PatTable));
    if(patTable==NULL)
    {
        printf("\n%s : ERROR Cannot allocate memory\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }  
    memset(patTable, 0x0, sizeof(PatTable));

    /* allocate memory for PMT table section */
    pmtTable=(PmtTable*)malloc(sizeof(PmtTable));
    if(pmtTable==NULL)
    {
        printf("\n%s : ERROR Cannot allocate memory\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }
    memset(pmtTable, 0x0, sizeof(PmtTable));

    /* allocate memory for TDT table section */
    tdtTable=(TdtTable*)malloc(sizeof(TdtTable));
    if(tdtTable==NULL)
    {
        printf("\n%s : ERROR Cannot allocate memory\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }  
    memset(tdtTable, 0x0, sizeof(TdtTable));

    /* allocate memory for TOT table section */
    totTable=(TotTable*)malloc(sizeof(TotTable));
    if(totTable==NULL)
    {
        printf("\n%s : ERROR Cannot allocate memory\n", __FUNCTION__);
        return (void*) SC_ERROR;
    }  
    memset(totTable, 0x0, sizeof(TotTable));

       
    /* initialize tuner device */
    if(Tuner_Init())
    {
        printf("\n%s : ERROR Tuner_Init() fail\n", __FUNCTION__);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        return (void*) SC_ERROR;
    }
    
    /* register tuner status callback */
    if(Tuner_Register_Status_Callback(tunerStatusCallback))
    {
        printf("\n%s : ERROR Tuner_Register_Status_Callback() fail\n", __FUNCTION__);
    }
    
    /* lock to frequency */
    if(!Tuner_Lock_To_Frequency(configFile.tuneFrequency, configFile.tuneBandwidth, configFile.tuneModule))
    {
        printf("\n%s: INFO Tuner_Lock_To_Frequency(): %d Hz - success!\n",__FUNCTION__, configFile.tuneFrequency);
    }
    else
    {
        printf("\n%s: ERROR Tuner_Lock_To_Frequency(): %d Hz - fail!\n",__FUNCTION__, configFile.tuneFrequency);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        Tuner_Deinit();
        return (void*) SC_ERROR;
    }
    
    /* wait for tuner to lock */
    pthread_mutex_lock(&statusMutex);
    if(ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime))
    {
        printf("\n%s : ERROR Lock timeout exceeded!\n",__FUNCTION__);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        Tuner_Deinit();
        return (void*) SC_ERROR;
    }
    pthread_mutex_unlock(&statusMutex);
   
    /* initialize player */
    if (Player_Init(&playerHandle))
    {
        printf("\n%s : ERROR Player_Init() fail\n", __FUNCTION__);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        Tuner_Deinit();
        return (void*) SC_ERROR;
    }

    /* set initial volume value */
    if (Player_Volume_Set(playerHandle, currentVolume*volumeConstant))
    {
        printf("\n%sError setting volume", __FUNCTION__);
    }
    
    /* open source */
    if (Player_Source_Open(playerHandle, &sourceHandle))
    {
        printf("\n%s : ERROR Player_Source_Open() fail\n", __FUNCTION__);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return (void*) SC_ERROR;    
    }

    /* set PAT pid and tableID to demultiplexer */
    if(Demux_Set_Filter(playerHandle, 0x00, 0x00, &filterHandle))
    {
        printf("\n%s : ERROR Demux_Set_Filter() fail\n", __FUNCTION__);
    }
    
    /* register section filter callback */
    if(Demux_Register_Section_Filter_Callback(sectionReceivedCallback))
    {
        printf("\n%s : ERROR Demux_Register_Section_Filter_Callback() fail\n", __FUNCTION__);
    }

    pthread_mutex_lock(&demuxMutex);
    if (ETIMEDOUT == pthread_cond_wait(&demuxCond, &demuxMutex))
    {
        printf("\n%s:ERROR Lock timeout exceeded!\n", __FUNCTION__);
        free(patTable);
        free(pmtTable);
        free(tdtTable);
        free(totTable);
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return (void*) SC_ERROR;
    }
    pthread_mutex_unlock(&demuxMutex);
    
    /* start current channel */
    startChannel(programNumber);
    
    /* set isInitialized flag */
    isInitialized = true;

    pthread_mutex_lock(&initMutex);
    pthread_cond_signal(&initCond);
    pthread_mutex_unlock(&initMutex);

    while(!threadExit)
    {
        if (changeChannel)
        {
            changeChannel = false;
            startChannel(programNumber);
        }
    }
}

int32_t sectionReceivedCallback(uint8_t *buffer)
{
    uint8_t tableId = *buffer;  
    if(tableId==0x00)
    {
        //printf("\n%s -----PAT TABLE ARRIVED-----\n",__FUNCTION__);
        
        if(parsePatTable(buffer,patTable)==TABLES_PARSE_OK)
        {
            //printPatTable(patTable);
            pthread_mutex_lock(&demuxMutex);
            pthread_cond_signal(&demuxCond);
            pthread_mutex_unlock(&demuxMutex);
            
        }
    } 
    else if (tableId==0x02)
    {
        //printf("\n%s -----PMT TABLE ARRIVED-----\n",__FUNCTION__);
        
        if(parsePmtTable(buffer,pmtTable)==TABLES_PARSE_OK)
        {
            //printPmtTable(pmtTable);
            pthread_mutex_lock(&demuxMutex);
            pthread_cond_signal(&demuxCond);
            pthread_mutex_unlock(&demuxMutex);
        }
    }
    else if (tableId == 0x70)
    {
        //printf("\n%s -----TDT TABLE ARRIVED-----\n",__FUNCTION__);

        if (parseTdtTable(buffer, tdtTable) == TABLES_PARSE_OK)
        {
            //printTdtTable(tdtTable);
            pthread_mutex_lock(&demuxMutex);
            pthread_cond_signal(&demuxCond);
            pthread_mutex_unlock(&demuxMutex);
        }
    }
    else if (tableId == 0x73)
    {
        //printf("\n%s -----TOT TABLE ARRIVED-----\n",__FUNCTION__);

        if (parseTotTable(buffer, totTable) == TABLES_PARSE_OK)
        {
            //printTotTable(totTable);
            pthread_mutex_lock(&demuxMutex);
            pthread_cond_signal(&demuxCond);
            pthread_mutex_unlock(&demuxMutex);
        }
    }

    return 0;
}

int32_t tunerStatusCallback(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n%s -----TUNER LOCKED-----\n",__FUNCTION__);
    }
    else
    {
        printf("\n%s -----TUNER NOT LOCKED-----\n",__FUNCTION__);
    }
    return 0;
}

StreamControllerError loadInitialInfo(char* fileName)
{
    if (loadConfigFile(fileName, &configFile))
    {
        return SC_ERROR;
    }

    programNumber = configFile.programNumber;

    return SC_NO_ERROR;
}

StreamControllerError loadConfigFile(char* filename, InitialInfo* configInfo)
{
    FILE* inputFile;
    char singleLine[LINE_LENGTH];
    char* singleWord;

    if ((inputFile = fopen(filename, "r")) == NULL)
    {
        return SC_ERROR;
    }

    while (fgets(singleLine, LINE_LENGTH, inputFile) != NULL)
    {
        singleWord = strtok(singleLine, "-");

        removeWhiteSpaces(singleWord);

        if (strcmp(singleWord, "frequency") == 0)
        {
            singleWord = strtok(NULL, "-");
            removeWhiteSpaces(singleWord);
            configInfo->tuneFrequency = atoi(singleWord);
        }
        else if (strcmp(singleWord, "bandwidth") == 0)
        {
            singleWord = strtok(NULL, "-");
            removeWhiteSpaces(singleWord);
            configInfo->tuneBandwidth = atoi(singleWord);
        }
        else if (strcmp(singleWord, "module") == 0)
        {
            singleWord = strtok(NULL, "-");
            removeWhiteSpaces(singleWord);

            if (strcmp(singleWord, "DVB_T"))
            {
                configInfo->tuneModule = DVB_T;
            }
            else
            {
                printf("DTV standard not supported!\n");
                return SC_ERROR;
            }
        }
        else if (strcmp(singleWord, "program_number") == 0)
        {
            singleWord = strtok(NULL, "-");
            removeWhiteSpaces(singleWord);
            configInfo->programNumber = atoi(singleWord);
        }
    }

    fclose(inputFile);

    return SC_NO_ERROR;
}

static void removeWhiteSpaces(char* word)
{
    int stringLen = strlen(word);
    int i = 0;
    int j = 0;
    int k = stringLen - 1;
    char* startString = word;
    char* returnString;

    while (startString[i] == 32)
    {
        i++;
    }

    while ((startString[k] == 32) & (startString[k] != '\0'))
    {
        k--;
    }

    for (j = 0; j < (k - i); j++)
    {
        word[j] = startString[j+i];
    }

    word[k-i+1] = '\0';
}

StreamControllerError changeChannelKey(uint16_t channelNumber)
{
    if ((channelNumber > -1) && (channelNumber < patTable->serviceInfoCount))
    {
        programNumber = channelNumber;
        changeChannel = true;
    }

    return SC_NO_ERROR;
}

StreamControllerError registerTimeCallback(TimeCallback timeCallback)
{
    if (timeCallback == NULL)
    {
        printf("Error registring time callback!\n");
        return SC_ERROR;
    }
    else
    {
        printf("Time callback function registered!\n");
        timeRecievedCallback = timeCallback;
    }

    return SC_NO_ERROR;
}

StreamControllerError unregisterTimeCallback()
{
    timeRecievedCallback = NULL;

    return SC_NO_ERROR;
}

StreamControllerError registerVolumeCallback(VolumeCallback volumeCallback)
{
    if (volumeCallback == NULL)
    {
        printf("Error registring volume callback!\n");
        return SC_ERROR;
    }
    else
    {
        printf("Volume callback function registered!\n");
        volumeReportCallback = volumeCallback;
    }

    return SC_NO_ERROR;
}

StreamControllerError unregisterVolumeCallback()
{
    volumeReportCallback = NULL;

    return SC_NO_ERROR;
}

StreamControllerError registerProgramTypeCallback(ProgramTypeCallback programTypeCallback)
{
    if (programTypeCallback == NULL)
    {
        printf("Error registring volume callback!\n");
        return SC_ERROR;
    }
    else
    {
        printf("Volume callback function registered!\n");
        programType = programTypeCallback;
    }

    return SC_NO_ERROR;
}

StreamControllerError unregisterProgramTypeCallback()
{
    programType = NULL;

    return SC_NO_ERROR;
}

StreamControllerError volumeUp()
{
    if (++currentVolume > 10)
    {
        currentVolume = 10;
    }

    if (Player_Volume_Set(playerHandle, currentVolume*volumeConstant))
    {
        printf("\n%sError changing volume", __FUNCTION__);
    }

    volumeReportCallback(currentVolume);

    return SC_NO_ERROR;
}

StreamControllerError volumeDown()
{
    if (currentVolume > 0)
    {
        currentVolume--;
    }

    if (Player_Volume_Set(playerHandle, currentVolume*volumeConstant))
    {
        printf("\n%sError changing volume", __FUNCTION__);
    }

    volumeReportCallback(currentVolume);

    return SC_NO_ERROR;
}

StreamControllerError volumeMute()
{
    if (Player_Volume_Set(playerHandle, 0))
    {
        printf("\n%sError changing volume", __FUNCTION__);
    }

    currentVolume = 0;

    return SC_NO_ERROR;
}
