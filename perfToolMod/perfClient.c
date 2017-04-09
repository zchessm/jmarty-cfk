/*********************************************************
* Module Name: perfClient source
*
* File Name:    perfClient.c
*
* Summary:
*  This file contains the client portion of a client/server
*         UDP-based performance tool.
*
*  char *  serverIP = argv[1]; 
*  unsigned short  serverPort = atoi(argv[2]);
*  uint  timeLimit = atoi(argv[3]);
*  uint  averageRate = atoi(argv[4]);
*  uint  bucketSize = atoi(argv[5]);
*  uint  tokenSize = atoi(argv[6]);
*  uint  messageSize = atoi(argv[7]);
*  uint  mode = atoi(argv[8]);
*          0: RTT mode
*         
*  uint  numberIterations = atoi(argv[8]);
*  uint  debugFlag = atoi(argv[9]);  //used to set debugLevel
*
*    fprintf(stderr, "perfClient(v%s): <Server IP> <Server Port> [<Time Limit(s)>] [<Average Rate>] "
*                    "[<Bucket Size>] [<Token Size>] [<Message Size>] "
*                    "[<Mode (RTT = 0, One-way = 1)>] [<# of iterations>] "
*                    "[<Debug Flag (default : 129)>]\n", Version);
*
*   debugFlag anded 0x000000ff = debugLevel
*    0:  nothing displayed except final result
*    1:  (default) adds errors and warning
*    2:  adds RTT samples
*    4:  some debug
*    5:  lots of debug
*    Set 8th bit -  creates RTT.dat
*
* Notes:
*  
*    
*
*********************************************************/
#include "perfTool.h"


int bStop = 0;
unsigned int debugFlag = 129;
unsigned int debugLevel = 1;
unsigned int createDataFileFlag = 0;

void CatchAlarm(int ignored) {}
void ClientStop() { bStop = 1; }

unsigned int receivedCount = 0;
long totalPing = 0;
long minPing = LONG_MAX;
long maxPing = 0;
long avgPing = 0;

uint16_t mode = PING_MODE;
uint16_t messageSize = 1000;
unsigned int messageCount = 0;
unsigned int totalMessageCount = 0;
unsigned int  seqNumberTimed = 1;
unsigned int timeLimit = 0;
struct timeval sessionStart, sessionEnd;

typedef struct serverResponse {
  unsigned int responseNum;
  long messagePing;
  struct serverResponse *next;
} serverResponse;

serverResponse *firstResponse = NULL;
serverResponse *lastResponse = NULL;

unsigned int numberResponses = 0;
unsigned int totalReceivedMsgs = 0;


/*************************************************************
* routine:  serverResponse *addResponse(unsigned int num, long ping) 
*
* Function: adds a RTT sample to the list 
*           
* inputs: 
*  serverResponse *addResponse(
*    unsigned int num :  sequence number associated with the sample
*    long ping :   the RTT sample
*
* outputs:  
*    Returns a ptr to the result that is saved 
*          Or a NULL on error.
*
* notes: 
*
***************************************************************/
serverResponse *addResponse(unsigned int num, long ping) 
{

  serverResponse *resp = malloc(1 * sizeof(serverResponse));
  if (resp == NULL) {
    printf("perfClient: HARD ERROR Failed malloc serverResponse buffer of %d bytes\n",(unsigned int) sizeof(serverResponse));
    return resp;
  }
  resp->responseNum = num;
  resp->messagePing = ping;
  resp->next = NULL;

  if (firstResponse == NULL) 
  {
    firstResponse = lastResponse = resp;
  } else 
  {
    lastResponse->next = resp;
    lastResponse = resp;
  }
  numberResponses++;
  return resp;

}

void freeResponses() {
  serverResponse *tofree;
  while (firstResponse != NULL) {
    tofree = firstResponse;
    firstResponse = firstResponse->next;
    free(tofree);
    numberResponses--;
  }
}

double getPingStdDev() 
{
double avgDiff = 0.0;
double stddev = 0.0;

  serverResponse *toeval = firstResponse;
  long meanDiff;
  long sumSqrdDiffs = 0;
  while (toeval != NULL) 
  {
    meanDiff = toeval->messagePing - avgPing;
    sumSqrdDiffs += (meanDiff * meanDiff);
    toeval = toeval->next;
  }
  if (receivedCount > 0) 
  {
    avgDiff = sumSqrdDiffs / (float)receivedCount;
    stddev = sqrt(avgDiff) / 1000000.0;
    if (debugLevel > 2) {
      printf("perfClient: rxcount:%d, sumSqrdDiffs:%ld, avgDiff:%f, stddev:%f \n", 
          receivedCount,sumSqrdDiffs,avgDiff,stddev);
    }
  } 
  return stddev;
}

void setUnblockOption(int sock, char unblock) {
  int opts = fcntl(sock, F_GETFL);
  if (unblock == 1)
    opts |= O_NONBLOCK;
  else
    opts &= ~O_NONBLOCK;
  fcntl(sock, F_SETFL, opts);
}

void sockBlockingOn(int sock) { setUnblockOption(sock, 0); }
void sockBlockingOff(int sock) { setUnblockOption(sock, 1); }

FILE *newFile = NULL;
char dataFile[] = "RTT.dat";
double startTime = 0.0;
double endTime = 0.0;
double  duration = 0.0;
double curTime = 0.0;


/*************************************************************
*
* Function: Main program for perfClient 
*           
* inputs: 
*  char *  serverIP = argv[1]; 
*  unsigned int  serverPort = atoi(argv[2]);
*  uint  averageRate = atoi(argv[3]);
*  uint  bucketSize = atoi(argv[4]);
*  uint  tokenSize = atoi(argv[5]);
*  uint  messageSize = atoi(argv[6]);
*  uint  mode = atoi(argv[7]);
*  uint  numberIterations = atoi(argv[8]);
*  uint  debugFlag = atoi(argv[9]);
*
* outputs:  
*
* notes: 
*
***************************************************************/
int main(int argc, char *argv[]) 
{
  int sock = -1;                         /* Socket descriptor */
  struct sockaddr_in serverAddress; /* Echo server address */
  struct sockaddr_in clientAddress; /* Source address of echo */
  unsigned short serverPort;        /* Echo server port */
  unsigned int clientAddressSize = 0;   /* In-out of address size for recvfrom() */
  char *serverIP = NULL;                   /* IP address of server */
  struct hostent *thehost = NULL;             /* Hostent from gethostbyname() */
  struct timeval currentTime, lastTokenTime;
  struct timeval sentTime, receivedTime, lastSentTime;
  struct sigaction myaction;
  long messagePing = 0;
  unsigned int seqNumber = 1;
  unsigned int RxSeqNumber = 1;
  unsigned int averageRate = 1000000;
  unsigned int tokenSize = 1000;
  unsigned int bucketSize = 10000;
  unsigned int numberIterations = 1;
  int bucket = 0;
  double tokenAddInterval=0;
  double RTTSample = 0.0;
  double OWDelayTimeFromPeer = 0.0;
  double OWDelayTimeEstimate = 0.0;
  double smoothedOWDelay = 0.0;
 
  fd_set rcvset;   //init just before the call to select
  struct timeval myOffTimeVal; 
  double myOffTime = 0; 

  double numberSelects = 0;
  double numberSelectsNEVER=0;
  double numberSelectsNOERROR = 0;
  double numberSelectsWITHERROR = 0;
  double numberSelectsWITHTO = 0;
  double numberSelectsWithISSET = 0;
  double numberSelectsWITHRECVERROR = 0;


  char *RxBuffer = NULL;
  char *SendBuffer = NULL;

  unsigned int messagesWaiting = 0;
  int numberMsgsInFlight  = 0;
  unsigned int largestSeqSent = 0;
  unsigned int largestSeqRecv = 0;
  int rc  = SUCCESS;
  int burstAmount = 0;
  int burstCount = 0;

  //Monitor avg burst Size 
  double totalBurstCount=0.0; //number bytes in each burst
  unsigned int numberBursts = 0;

  if (argc < 3) /* Test for min number arguments */
  {
    fprintf(stderr, "perfClient(v%s): <Server IP> <Server Port> [<Time Limit(s)>] [<Average Rate>] "
                    "[<Bucket Size>] [<Token Size>] [<Message Size>] "
                    "[<Mode (RTT = 0, One-way = 1)>] [<# of iterations>] "
                    "[<Debug Flag (default : 129)>]\n", Version);
    exit(1);
  }

  signal(SIGINT, ClientStop);
  /*Get starting time.... */
  startTime = timestamp();


  /* First arg: server IP address (dotted quad) or domain name....but either
                case the param is a string  */
  serverIP = argv[1]; 
  serverPort = atoi(argv[2]);

  // get info from parameters if specified:
	if (argc >= 4)
		timeLimit = atoi(argv[3]) * 1000000;

  if (argc >= 5)
    averageRate = atoi(argv[4]);

  if (argc >= 6)
    bucketSize = atoi(argv[5]);

  if (argc >= 7)
    tokenSize = atoi(argv[6]);

  if (argc >= 8) {
    messageSize = atoi(argv[7]);
    if (messageSize < MESSAGEMIN || messageSize > MESSAGEMAX) {
      printf("perfClient: HARD ERROR Message Size must be between %d and %d bytes\n", MESSAGEMIN, MESSAGEMAX);
      exit(1);
    }
  }

  if (argc >= 9)
    mode = atoi(argv[8]);

  if (argc >= 10)
    numberIterations = atoi(argv[9]);

  if (numberIterations == 0)
    numberIterations = UINT_MAX;

  if (argc >= 11)
    debugFlag = atoi(argv[10]);

  // Calculate the token add interval in microseconds:
  double tokensPerSecond = (averageRate / 8.0) / tokenSize;
  tokenAddInterval = (double)1000000 / tokensPerSecond;
  myOffTime = tokenAddInterval; 
  myOffTimeVal.tv_sec=0;
  myOffTimeVal.tv_usec=myOffTime;

  if (debugFlag >= 128) {
    createDataFileFlag = 1;
    debugLevel = debugFlag - 128; 
  } else {
    createDataFileFlag = 0;
    debugLevel = debugFlag;
  }


  if (debugLevel > 0) {
    printf("perfClient: %s:%d timeLimit:%d, averageRate:%d, bSize:%d, tSize:%d, msgSize:%d, mode:%d, numberIterations:%d, tokenInterval:%2.6f (debugLevel:%d) \n",
       serverIP, serverPort,
       timeLimit,
       averageRate, bucketSize, tokenSize, messageSize, 
       mode, numberIterations,
       tokenAddInterval/1000000, debugLevel);
  }

  if (createDataFileFlag == 1 ) {
    newFile = fopen(dataFile, "w");
    if ((newFile ) == NULL) {
      printf("perfClient: HARD ERROR failed fopen of file %s,  errno:%d \n", 
          dataFile,errno);
      exit(1);
    }
  }

  clientAddressSize = sizeof(clientAddress);
//  memset(&clientAddress, 0, clientAddressSize); /* Zero out structure */
  myaction.sa_handler = CatchAlarm;
  if (sigfillset(&myaction.sa_mask) < 0)
    DieWithError("sigfillset() failed");

  myaction.sa_flags = 0;

  if (sigaction(SIGALRM, &myaction, 0) < 0)
    DieWithError("sigaction failed for sigalarm");

  //setup the rx buffer
  RxBuffer = malloc(MESSAGEMAX); 
  if (RxBuffer == NULL) {
    printf("perfClient: HARD ERROR Failed on malloc of %d byte rx buffer\n", MESSAGEMAX);
    exit(1);
  }
  memset(RxBuffer, 0, MESSAGEMAX);

  /* Set up the send message */
  SendBuffer = malloc(MESSAGEMAX);
  if (SendBuffer == NULL) {
    printf("perfClient: HARD ERROR malloc of %d bytes send buffer \n", MESSAGEMAX);
    exit(1);
  }
  memset(SendBuffer, 0, MESSAGEMAX);

  //Will point to Send Buffer
  messageHeader *SendHeader = (messageHeader *)SendBuffer;
  SendHeader->size = htons(messageSize);
  SendHeader->mode = htons(mode);
  SendHeader->timeRxSeconds = 0;
  SendHeader->timeRxUSeconds = 0;

  //Will point to Rx Buffer
  messageHeader *RxHeader = (messageHeader *)RxBuffer;

	
  /* Construct the server address structure */
  memset(&serverAddress, 0, sizeof(serverAddress)); /* Zero out structure */
  serverAddress.sin_family = AF_INET;               /* Internet addr family */
  serverAddress.sin_addr.s_addr = inet_addr(serverIP); /* Server IP address */

  /* If user gave a dotted decimal address, we need to resolve it  */
  if (serverAddress.sin_addr.s_addr == -1) {
    thehost = gethostbyname(serverIP);
    if (thehost == NULL) {
      printf("perfClient: HARD ERROR bad address param %s, port:%d, h_errno:%d \n",serverIP,serverPort,h_errno);
      exit(1);
    }
    serverAddress.sin_addr.s_addr = *((unsigned long *)thehost->h_addr_list[0]);
  }

  serverAddress.sin_port = htons(serverPort); /* Server port */

  /* Create a datagram/UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");

  /* set up receive set */
  FD_ZERO(&rcvset);
  FD_SET(sock, &rcvset);

  gettimeofday(&sessionStart, NULL);
  lastTokenTime = sessionStart;
  lastSentTime = sessionStart;
  numberResponses=0;

  // sockBlockingOff(sock);
  // Loops for desired # of iterations or if an error occurs
  // messageCount tracks # of sends.
  // messagesWaiting tracks # of messages sent but not acked
  while (bStop != 1) {
    if (messageCount > numberIterations) 
    {
      bStop=0;
      break;
    }

    // Check to Add token  
    // Add token if tokenAddInterval has passed...
    gettimeofday(&currentTime, NULL);
    if (getTimeSpan(&lastTokenTime, &currentTime) >= tokenAddInterval) 
    {
      // ...and bucket still has capacity
      if (bucket + tokenSize <= bucketSize)
        bucket += tokenSize;
      else
        bucket = bucketSize;

      lastTokenTime = currentTime;
    }

    //prep for next burst....send up to a bucket of data....
    burstCount = 0;
    burstAmount = bucket;

    while (burstAmount >= messageSize  && messageCount < numberIterations) 
    {

      if (debugLevel > 3) {
        if (burstCount == 0) {
          printf("perfClient: BEGIN BURST  iteration:%d Begin burstNumber:%d, burstAmount:%d (next SeqNumber:%d, seqNumberTimed:%d)\n", 
            messageCount, numberBursts+1, burstAmount,seqNumber,seqNumberTimed);
        }
      }

      burstAmount -= messageSize;
      burstCount += messageSize;


      gettimeofday(&lastSentTime, NULL);
//      SendHeader->timeSent = lastSentTime;
      SendHeader->timeSentSeconds = htonl((uint32_t)lastSentTime.tv_sec);
      SendHeader->timeSentUSeconds = htonl((uint32_t)lastSentTime.tv_usec);
      SendHeader->sequenceNum = htonl(seqNumber);
      SendHeader->timeRxSeconds = 0; 
      SendHeader->timeRxUSeconds = 0;
      SendHeader->OWDelay = smoothedOWDelay;

      largestSeqSent = seqNumber;
      numberMsgsInFlight = largestSeqSent - largestSeqRecv;

      rc =sendto(sock, SendBuffer, messageSize, 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
      if (rc == messageSize) 
      {
        bucket -= messageSize;
        messageCount++;
        messagesWaiting++;
        if (debugLevel > 4) {
          printf("perfClient: Sent seqNumber:%d,totalSent:%d  messageSize:%d, updatedBucket:%d  \n", seqNumber, messageCount, messageSize,bucket);
        }

        //Should never happen- REMOVE
        if (bucket < 0) {
          printf("perfClient: HARD ERROR Sent seqNumber:%d, messageSize:%d, updatedBucket:%d  \n", seqNumber,messageSize,bucket);
          exit(1);
        }
        seqNumber++;

      } else {
        printf("perfClient: HARD ERROR Failure on sendTo, rc:%d, errno:%d\n",
               rc, errno);
//        if (errno == EMSGSIZE)
//          printf("perfClient: HARD ERROR  Message Size of %d is too big\n", messageSize);
        exit(1);
      }
    }

    //tally what was sent in the burst (if any)
    if (burstCount > 0) {
      totalBurstCount += (double)burstCount;
      numberBursts++;

      if (debugLevel > 3) {
        printf("perfClient: END BURST1 #burstCount:%d seqNumber:%d,updatedBucket:%d, totalSent:%d, totalReceived:%d,#inflight:%d  \n", 
          burstCount, seqNumber, bucket, messageCount, totalReceivedMsgs, numberMsgsInFlight);
        printf("perfClient: END BURST2:  #Selects:%6.0f, #NEVER:%9.0f, #NOERROR:%6.0f, #WITHERROR:%6.0f, #WITHTO:%6.0f, #WithISSET:%6.0f, #WITHRECVERROR:%6.0f \n", 
           numberSelects, numberSelectsNEVER, numberSelectsNOERROR, numberSelectsWITHERROR, numberSelectsWITHTO, numberSelectsWithISSET, numberSelectsWITHRECVERROR);
      }

    }

    gettimeofday(&currentTime, NULL);
    curTime = convertTimeval(&currentTime);
    double partialDelay  = 1000000 * (curTime - convertTimeval(&lastSentTime));
    myOffTime = (tokenAddInterval - partialDelay)/1000000;
    myOffTimeVal.tv_sec= (uint32_t)floor(myOffTime);
    if (myOffTimeVal.tv_sec >= 1) 
      myOffTimeVal.tv_usec=  (uint32_t) (myOffTime - (double) myOffTimeVal.tv_sec);
    else 
      myOffTimeVal.tv_usec=  (uint32_t) (1000000 * myOffTime);


    if (debugLevel > 4) {
      printf("perfClient:TIMES: tokenAddInt:%f partialDelay:%f, myOffTime:%f TimeVal.tv_sec:%d tv_usec:%d\n",
         tokenAddInterval/1000000,partialDelay/1000000,myOffTime, (unsigned int)myOffTimeVal.tv_sec,(unsigned int) myOffTimeVal.tv_usec);
    }

    // Receive a response if in mode 0 (PING_MODE)
    if ((mode == PING_MODE) || (mode == LIMITED_RTT))
    {
     if (numberMsgsInFlight > 0) {
      //blocks until either the off time is done or a new ACK arrives 
      //Always reset and set as select() will modify them...
      FD_ZERO(&rcvset);
      FD_SET(sock, &rcvset);
      rc = select(sock+1, &rcvset, NULL, NULL, &myOffTimeVal);
      numberSelects++;
      if (rc == ERROR) {
        numberSelectsWITHERROR++;
        printf("perfClient: HARD ERROR on select (myOffTimeVal.Usecs:%d, errno:%d) \n", 
                       (unsigned int)myOffTimeVal.tv_usec,errno);
        //ignore for now
        rc =  SUCCESS;
       // exit(1);
      } else 
        numberSelectsNOERROR++;

      if(FD_ISSET(sock, &rcvset))
      {
        //Set to non-blocking
        //sockBlockingOff(sock);
        numberSelectsWithISSET++;;
        rc = recvfrom(sock, RxBuffer, messageSize, 0, (struct sockaddr *)&clientAddress, &clientAddressSize);
        if (rc >=0 ) 
        {
          if (rc != messageSize) {
            printf("perfClient: HARD ERROR partial receive (%d bytes, should have been %d bytes) \n", rc,messageSize);
            exit(1);
          }
          totalReceivedMsgs++;
  
          RxHeader = (messageHeader *)RxBuffer;
          RxSeqNumber = ntohl(RxHeader->sequenceNum);
          if (largestSeqRecv < RxSeqNumber)
            largestSeqRecv = RxSeqNumber;

          numberMsgsInFlight = largestSeqSent - largestSeqRecv;
 
          if (RxSeqNumber >= seqNumberTimed ) { 
            seqNumberTimed = largestSeqSent;
            gettimeofday(&receivedTime, NULL);
            curTime = convertTimeval(&receivedTime);
            sentTime.tv_sec = (ntohl(RxHeader->timeSentSeconds));
            sentTime.tv_usec = (ntohl(RxHeader->timeSentUSeconds));
            double MyMsgSentTime = ((ntohl(RxHeader->timeSentSeconds * 1000000)) + ntohl(RxHeader->timeSentUSeconds));
            double RxSendTime = ((ntohl(RxHeader->timeRxSeconds * 1000000)) + ntohl(RxHeader->timeRxUSeconds));
            OWDelayTimeFromPeer = ntohl(RxHeader->OWDelay);
             OWDelayTimeEstimate = curTime - RxSendTime;
            smoothedOWDelay  = 0.5 * smoothedOWDelay + 0.5*OWDelayTimeEstimate;

            RTTSample =   curTime - MyMsgSentTime;
            messagePing = getTimeSpan(&sentTime, &receivedTime);
            messagesWaiting--;
            //totalReceivedMsgs++;

            if (debugLevel > 3) {
              printf("perfClient:RTT TIMES: rxhdr-secs:%d usecs:%d  MsgSendTime:%f RxSendTime:%f \n",
                (unsigned int)RxHeader->timeSentSeconds,(unsigned int) RxHeader->timeSentUSeconds,
              MyMsgSentTime,RxSendTime);
              printf("perfClient:RTT TIMES:  RTTSample:%f msgPing:%ld OWDelayFromPeer:%f OWDelaySmoothedEstimate:%f \n",
              RTTSample, messagePing, OWDelayTimeFromPeer, smoothedOWDelay);
            }

            if (createDataFileFlag == 1 ) {
              fprintf(newFile,"%4.6f %4.6f %4.6f %d %d %d %d \n", 
                convertTimeval(&receivedTime), ((double)messagePing)/1000000,  
                ((double)smoothedOWDelay)/1000000,  
                RxSeqNumber, largestSeqSent, largestSeqRecv, messagesWaiting);
            }

            addResponse(RxSeqNumber, messagePing);

            // Update stats
            receivedCount += 1;
            totalPing += messagePing;
            if (messagePing < minPing)
              minPing = messagePing;
            if (messagePing > maxPing)
              maxPing = messagePing;
            avgPing = totalPing / receivedCount;
          }
        } else {
          numberSelectsWITHRECVERROR++;
          //return of EAGAIN is expected since nonblocking and no data
          //if (errno != EAGAIN) {
            printf("perfClient: HARD ERROR Failure on recvFrom  errno:%d\n", errno);
            exit(1);
          //}
        }
      } else //else socket not active must be timeout....
         numberSelectsWITHTO++; 
   
     } else  // the else will happen -  no need to do anything.  
       numberSelectsNEVER++; 
      
    } else 
    {
      //otherwise just nanosleep until ready to send
      struct timespec reqDelay, remDelay;
      reqDelay.tv_sec = (long)myOffTimeVal.tv_sec;
      reqDelay.tv_nsec = (long)myOffTimeVal.tv_usec*1000;
      remDelay.tv_sec = 0;
      remDelay.tv_nsec = 0;
      rc = nanosleep((const struct timespec*)&reqDelay, &remDelay);
      if (rc == ERROR) {
        printf("perfClient: HARD ERROR nanosleep (timeout:%f, remDelayusec:%ld  errno:%d\n", 
               myOffTime*1000000,remDelay.tv_nsec/1000, errno);
        //exit(1);
        rc = SUCCESS;
      }
    }

    gettimeofday(&currentTime, NULL);
    //every so many iterations, check to see if time is up		
		totalMessageCount += messageCount;
		if (messageCount >= numberIterations &&
        numberMsgsInFlight == 0 )
    {
			//time is up if the time span from start to now is over the time limit
			if (getTimeSpan(&sessionStart, &currentTime) > timeLimit)
			{
      	bStop = 1;
			}
			//else, set everything back
			else
			{
				messageCount = 0;
			}
    }
  }

  /* Wrap up */
  gettimeofday(&sessionEnd, NULL);
  endTime = timestamp();
  duration = endTime - startTime;
  printf("perfClient: Ending #iteration:%d, duration(seconds):%f, #Selects:%f, #selectsERROR:%f \n",
         totalMessageCount,duration, numberSelects, numberSelectsNOERROR);

  // Send final message:
  SendHeader->sequenceNum = 0xffffffff;
  sendto(sock, SendBuffer, messageSize, 0, (struct sockaddr *)&serverAddress,
         sizeof(serverAddress));

  close(sock);

  long bytesSent = totalMessageCount * messageSize;
  float totalSeconds = getTimeSpan(&sessionStart, &sessionEnd) / 1000000.0;
  unsigned int sendingRate = 0;
  if (totalSeconds > 0)
    sendingRate = (bytesSent * 8) / totalSeconds;

  double lossPercent = 0.0;
  double lossPercent2 = 0.0;
  //messagesWaiting is the number not ack'ed
  //# outstanding: largestSeqSent - largestSeqRecv
  numberMsgsInFlight = largestSeqSent - largestSeqRecv;
  messagesWaiting -=numberMsgsInFlight;


  if (debugLevel > 0) {
    printf("perfClient:DONE1: sent:%d, rx:%d, waiting::%d (#inflight:%d, lost:%d) sendingRate:%d, numberResponses:%d \n", 
         totalMessageCount,receivedCount,messagesWaiting,numberMsgsInFlight, (totalMessageCount-receivedCount),sendingRate,numberResponses);
    printf("perfClient: DONE2: #Selects:%6.0f, #NEVER:%9.0f, #NOERROR:%6.0f, #WITHERROR:%6.0f, #WITHTO:%6.0f, #WithISSET:%6.0f, #WITHRECVERROR:%6.0f \n", 
           numberSelects, numberSelectsNEVER, numberSelectsNOERROR, numberSelectsWITHERROR, numberSelectsWITHTO, numberSelectsWithISSET, numberSelectsWITHRECVERROR);

  }
  double avgBurst = 0.0;
  if (numberBursts > 0)
     avgBurst = totalBurstCount / numberBursts;

  if (mode == 0) {
    double pMin = minPing / 1000000.0;
    double pMax = maxPing / 1000000.0;
    double pAvg = avgPing / 1000000.0;
    double pSD = getPingStdDev();
    if (totalMessageCount > 0) {
      lossPercent = (double)messagesWaiting / (double)totalMessageCount;
      lossPercent2 = (double)(totalMessageCount-totalReceivedMsgs) / (double)totalMessageCount;
    }

    printf("#\tmin\tmax\t\tavg\tstd\tloss1\tloss2\trate\t#bursts\tavgBurstSize(bytes)\t#Rxed\t#InFlight \n");
    printf("%d  %f   %f  %f\t%f\t%1.4f\t%1.4f\t%d\t%d\t%d\t%d\t%d\n", 
             totalMessageCount, pMin, pMax, pAvg, pSD, lossPercent,lossPercent2,sendingRate, 
             numberBursts,(int)avgBurst, (int)totalReceivedMsgs, numberMsgsInFlight);
  } else 
  {
    printf("perfClient: #sent sendrate #bursts avgBurst \n");
    printf("%d  %d\t%d\t%d\n", 
             totalMessageCount, sendingRate, numberBursts,(int)avgBurst);
  }
  freeResponses();
  if (createDataFileFlag == 1 ) {
    fclose(newFile);
  }
  exit(0);
}
