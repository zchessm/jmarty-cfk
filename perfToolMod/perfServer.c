/*********************************************************
* Module Name: perfServer source
*
* File Name:    perfServer.c
*
* Summary:
*  This file contains the Server portion of a client/server
*         UDP-based performance tool.
*
* Program parameters: 
*   UDP Port : specifies the servers UDP port (0-64K)
*   Emulated Avg Loss Rate : If specified, represents the
*    target loss rate of a random uncorrelated loss generator.
*   AckStrategy : specifies how many arrivals per an ack (for mode 0)
*                 Value of 1 is normal echo mode, value of 2 echoes every other, ...
*   debug Flag :  specifies if debug messages are sent to standard out.
*            Value of 0 :  No output except for HARD ERRORS (which cause an exit)
*            Value of 1 :  Minimal output (default setting)
*            Value of 2 :  Verbose output (handy when debugging)
*  set debugFlag as follows:
*    0:  nothing displayed except final result
*    1:  (default) adds errors and warning
*    2:  adds RTT samples
*    4:  some debug
*    5:  lots of debug
*    Set 8th bit -  creates SRT.dat
*
*
* Notes:
*    The server maintains a record of new client sessions :
*    The list begins at firstSession 
*      with last element : lastSession
*      A new session is defined by a unique client IP/port  
*
* TODO:
*    Periodically delete sessions that have no activity for 
*        SESSION_INACTIVITY_TIME amount of seconds.
*        Can log to a db/file with a configuredo frequency
*        and remove inactive sessions then....
*  
*********************************************************/
#include "perfTool.h"

int sock;
FILE *newFile = NULL;
//Serverside times
char dataFile[] = "SST.dat";



double startTime = 0.0; //time when we started the program
double firstSessionTime =  -1;  //time when first client msg received
double lastSessionTime =  -1;   //time of the last msg was received
double curTime = 0.0; 


//Set 0 for NO STOP,  1 if stopped
int bStop = 0;
void ServerStop() {
  bStop = 1;
  close(sock);
}

typedef struct session {
  struct in_addr clientIP;
  unsigned short clientPort;
  struct timeval timeStarted;
  struct timeval lastActive;
  long duration;
  unsigned int bytesReceived;
  unsigned int messagesReceived;
  unsigned int messagesLost;
  unsigned int lastSequenceNum;
  unsigned int largestSeqRecv;
  struct session *prev;
  struct session *next;
} session;

session *firstSession = NULL;
session *lastSession = NULL;
session *firstActive = NULL;
unsigned int sessionCount = 0;

session *findActive(struct in_addr clientIP, unsigned short clientPort) {
  session *s = firstActive;
  while (s != NULL) {
    if (s->clientIP.s_addr == clientIP.s_addr && s->clientPort == clientPort)
      return s;
    s = s->next;
  }
  return s;
}

session *getActive(struct in_addr clientIP, unsigned short clientPort) {
  // Find active session
  session *s = findActive(clientIP, clientPort);
  // If matching session is not found...
  if (s == NULL) {
    // ...add new session
    s = malloc(1 * sizeof(session));
    s->clientIP = clientIP;
    s->clientPort = clientPort;
    gettimeofday(&(s->timeStarted), NULL);
    s->lastActive = s->timeStarted;
    s->lastSequenceNum = 0;
    s->bytesReceived = 0;
    s->messagesReceived = 0;
    s->messagesLost = 0;
    s->duration = 0;
    s->next = firstActive;
    s->prev = NULL;
    firstActive = s;
    sessionCount++;
  }
  return s;
}

void removeActive(struct in_addr clientIP, unsigned short clientPort) {
  session *s = findActive(clientIP, clientPort);
  if (s != NULL) {
    if (s->next != NULL)
      s->next->prev = s->prev;
    if (s->prev != NULL)
      s->prev->next = s->next;
    if (s == firstActive)
      firstActive = (s->prev != NULL) ? s->prev : s->next;
    // Add to archive list:
    s->next = NULL;
    if (firstSession == NULL) {
      firstSession = s;
      lastSession = s;
      s->prev = NULL;
    } else {
      lastSession->next = s;
      lastSession = s;
    }
  }
}

void updateSessionDuration(session *s) {
  struct timeval sessionEnd;
  gettimeofday(&sessionEnd, NULL);
  s->duration = getTimeSpan(&(s->timeStarted), &sessionEnd);
}

void printSessions() {
  session *toprint = firstSession;
  double durSecs = 0.0;
  unsigned int  bps = 0;
  double tmpVar = 0.0;
  double lossPercent = 0.0;

  printf("cAddr clientPort, durSecs/1000000, bytes numberRxed bps lossrate \n");
  while (toprint != NULL) {
    char *cAddr = inet_ntoa(toprint->clientIP);
    if (toprint->duration == 0)
      updateSessionDuration(toprint);
    durSecs = toprint->duration / 1000000.0;
    bps = (toprint->bytesReceived * 8) / durSecs;
    tmpVar = (double)(toprint->messagesLost + toprint->messagesReceived);
    lossPercent = 0.0;
    if (tmpVar > 0.0) 
      lossPercent = toprint->messagesLost /tmpVar;

    printf("%s %d %ld %d %d %d %d %1.4f\n", 
        cAddr, toprint->clientPort, 
        toprint->duration, toprint->lastSequenceNum,
        toprint->bytesReceived, toprint->messagesReceived, 
        bps, lossPercent);

    toprint = toprint->next;
  }
}

void freeAllSessions() {
  while (firstSession != NULL) {
    session *tofree = firstSession;
    firstSession = tofree->next;
    free(tofree);
  }
}


int main(int argc, char *argv[]) {  /* Socket */
  int rc = SUCCESS;
  struct sockaddr_in serverAddress; /* Local address */
  struct sockaddr_in clientAddress; /* Client address */
  unsigned int cliAddrLen = 0;          /* Length of incoming message */
  unsigned short serverPort;        /* Server port */
  int recvMsgSize = 0;                  /* Size of received message */
  messageHeader *header = NULL;
  int clientMode = 0;
  int clientSequenceNum = 0;
  int rcvdSize = 0;
  unsigned int debugFlag = 129;
  unsigned int debugLevel = 1;
  unsigned int createDataFileFlag = 0;
  unsigned int receivedCount = 0;
  unsigned int repliedCount = 0;
  unsigned int dropCount = 0;

  struct timeval localReceivedTime;
  double lastOWDelay = 0.0;
  double curOWDelay = 0.0;
  double curTime = 0.0;
  double delayChange = 0.0;
  double delaySum = 0.0;
  double smoothedDelayChange = 0.0;
  double avgLossRate = 0.0;
  unsigned int avgAggregateThroughput = 0;
  double totalBytesRxed = 0;
  int AckStrategy = 1;       //specifies how many arrivals per ack  
                            // 0 No acks ,   1 ping ECHO,  2 every other, ...
  int ArrivalsBeforeAck = 0;

  char *echoBuffer = NULL;         /* Buffer for Rx and then Send  */

  if (argc < 2) /* Test for correct number of parameters */
  {
    fprintf(stderr, "PerfServer(v%s):  <UDP Port> [<Emulated Avg Loss Rate>] [<AckStratgy [0-1000]>] [<Debug Flag (default 129)>]\n",
            Version);
    exit(1);
  }

  signal(SIGINT, ServerStop);
  startTime = timestamp();


  serverPort = atoi(argv[1]); /* First arg:  local port */
  if (argc >= 3)
    avgLossRate = atof(argv[2]);

  if (argc >= 4)
    AckStrategy = atoi(argv[3]);

  if (argc >= 5)
    debugFlag = atoi(argv[4]);

  if (debugFlag >= 128) {
    createDataFileFlag = 1;
    debugLevel = debugFlag - 128; 
  } else {
    createDataFileFlag = 0;
    debugLevel = debugFlag;
  }


  if (createDataFileFlag == 1 ) {
    newFile = fopen(dataFile, "w");
    if ((newFile ) == NULL) {
      printf("perfServer: HARD ERROR failed fopen of file %s,  errno:%d \n", 
          dataFile,errno);
      exit(1);
    }
  }

  echoBuffer = malloc(MESSAGEMAX);
  if (echoBuffer == NULL) {
    printf("perfServer:(v:%s): HARD ERROR on malloc (errno:%d) \n", (char *)Version, errno);
    exit(1);
  } 
  memset(echoBuffer, 0, MESSAGEMAX);

 
  if (debugLevel > 0)
    printf("perfServer:(v:%s): Port:%d, AvgLossRate:%2.4f,  AckStrategy:%d\n",
         (char *)Version, serverPort, avgLossRate, AckStrategy);

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) 
  {
    printf("perfServer:(v:%s): HARD ERROR:  Failed on socket (errno:%d)\n ", (char *)Version,errno);
    exit(1); 
  }

  /* Construct local address structure */
  memset(&serverAddress, 0, sizeof(serverAddress));                   /* Zero out structure */
  serverAddress.sin_family = AF_INET;                                 /* Internet address family */
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);                  /* Any incoming interface */
  serverAddress.sin_port = htons(serverPort);                         /* Local port */

  /* Bind to the local address */
  unsigned int bindAgain = TRUE;
  if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) 
  {
    printf("perfServer:(v:%s): HARD ERROR:  Failed on bind (errno:%d)\n ", (char *)Version,errno);
    exit(1); 
  } else {
    bindAgain = FALSE;
  }
  session *currentSession = NULL;
  srand(time(NULL));

  //Loop until:  sigint or error
  while (bStop != 1) 
  {
    /* Might need to try again in case of some error  */
    if (bindAgain == TRUE) 
    {
      rc = bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
      if (rc < 0) 
      {
        printf("perfServer:(v:%s): HARD ERROR:  Failed on bind (errno:%d)\n ", (char *)Version,errno);
        exit(1); 
      }
      printf("perfServer:(v:%s): WARNING: Needed to rebind ?? receivedCount:%d \n ",(char *)Version, receivedCount);
    }

    /* Set the size of the in-out parameter */
    cliAddrLen = sizeof(clientAddress);

    /* Block until receive message from a client */
    if ((recvMsgSize =
            recvfrom(sock, echoBuffer, MESSAGEMAX, 0,
                    (struct sockaddr *)&clientAddress, &cliAddrLen)) < 0) 
    {
      if (bStop != 1) {
        printf("perfServer: HARD ERROR failed recvfrom client:%s, errno: %d\n", inet_ntoa(clientAddress.sin_addr), errno);
        exit(1);
      } else 
        if (debugLevel > 2)
          printf("perfServer: WARNING: error on recvfrom and bStop == 1 (errno:%d) \n",errno);
    } else 
    {
      //Received data from a client ....
      lastSessionTime = timestamp();
      if (firstSessionTime == -1) {
        firstSessionTime = lastSessionTime;
      }
      receivedCount++;
      header = (messageHeader *)echoBuffer;
      clientMode = ntohs(header->mode);
      clientSequenceNum = ntohl(header->sequenceNum);
      rcvdSize = ntohs(header->size);
      totalBytesRxed += recvMsgSize;

      if (rcvdSize != recvMsgSize) {
        printf("perfServer: HARD ERROR rcvdSize!=recvMsgSize (%d, %d)\n",rcvdSize,recvMsgSize);
        exit(1);
      }
      if (debugLevel > 3) {
        printf("perfServer: Message received : recvfrom:%d bytes, msgHdr:%d \n",
           recvMsgSize,rcvdSize);
      }

      // Find session - will return a new one if not found
      currentSession =
          getActive(clientAddress.sin_addr, clientAddress.sin_port);

      gettimeofday(&localReceivedTime, NULL);
      curTime = convertTimeval(&localReceivedTime);
      currentSession->lastActive = localReceivedTime;

      struct timeval msgSentTime;
      msgSentTime.tv_sec = ntohl(header->timeSentSeconds);
      msgSentTime.tv_usec = ntohl(header->timeSentUSeconds);
      double msgSendTime = convertTimeval(&msgSentTime);

      
//TODO:  remove timeSent
      lastOWDelay = curOWDelay;
      curOWDelay  = msgSendTime - curTime;
      delayChange = curOWDelay - lastOWDelay;
      smoothedDelayChange = 0.5 * smoothedDelayChange + 0.5*delayChange;
      delaySum = delaySum + smoothedDelayChange;

      if (delaySum < 0) 
        delaySum = 0.0;

      header->OWDelay = htonl(curOWDelay);
      header->timeRxSeconds = htonl((unsigned int)localReceivedTime.tv_sec);
      header->timeRxUSeconds = htonl((unsigned int) localReceivedTime.tv_usec);

      if (createDataFileFlag == 1 ) {
        fprintf(newFile,"%4.6f %4.6f %d %d %d %4.6f %4.6f %4.6f %d\n", 
            curTime, curOWDelay, clientSequenceNum, currentSession->largestSeqRecv,rcvdSize, 
            delayChange, smoothedDelayChange, delaySum, rcvdSize);
//       fprintf(newFile,"%4.6f %4.6f %4.6f %4.6f %d %d %d\n", 
//            curTime, delayChange, smoothedDelayChange, delaySum, clientSequenceNum, currentSession->largestSeqRecv, rcvdSize);
      }
      // If this is the last message...
      if (clientSequenceNum == 0xffffffff) {
        // ... "close" the session
        if (debugLevel > 3) {
          printf("perfServer: Session ended \n" );
        }
        updateSessionDuration(currentSession);
        removeActive(clientAddress.sin_addr, clientAddress.sin_port);
      } else {
        // ... otherwise, update the session info
        currentSession->bytesReceived += recvMsgSize;
        currentSession->messagesReceived++;
        currentSession->messagesLost +=
            clientSequenceNum - currentSession->lastSequenceNum - 1;
        currentSession->lastSequenceNum = clientSequenceNum;
        if (currentSession->largestSeqRecv < clientSequenceNum)
          currentSession->largestSeqRecv = clientSequenceNum;
      }

      if (debugLevel > 3) {
        printf("perfServer: curTime:%4.6f OWDelay:%4.6f seqNum:%d \n",
            curTime, curOWDelay, clientSequenceNum);
        printf("perfServer: delayChange:%4.6f smoothedDC:%4.6f delaySum:%4.6f\n", 
            delayChange, smoothedDelayChange, delaySum);
      }
    }

    int sendFlag = FALSE;
    if (rc == SUCCESS) 
    {
      //Have received data , echo it if necessary
      if (clientMode == 0 )
      {
        if (avgLossRate == 0.0) 
            sendFlag = TRUE; 
        else 
        { 
          double myRandomNum = (double)rand() / (double)RAND_MAX;
          if (myRandomNum > avgLossRate)
            sendFlag = TRUE; 
          else
            dropCount++;
        }
        if (AckStrategy > 0)  {
          ArrivalsBeforeAck++;
          if (ArrivalsBeforeAck >= AckStrategy) {
            sendFlag = TRUE;
            ArrivalsBeforeAck=0;
          }
        }
        if (sendFlag == TRUE) {
          /* Send received datagram back to the client */
          if (sendto(sock, echoBuffer, recvMsgSize, 0,
                 (struct sockaddr *)&clientAddress,
                 sizeof(clientAddress)) != recvMsgSize) 
          {
            printf("perfServer: HARD ERROR failed sendto client:%s, errno: %d\n", inet_ntoa(clientAddress.sin_addr), errno);
            exit(1);
          }
          else 
          { 
            repliedCount++;
            if (debugLevel > 3) {
              printf("perfServer:  Sent Echo Back SeqNum: %d, repliedCount:%d, dropCount:%d  \n",
               clientSequenceNum,repliedCount,dropCount);
            }
          }

        }
      }
    } else 
    {
      printf("perfServer: HARD ERROR FAILED  \n");
      exit(1);
    }
  }

  /* Wrap up */
  double endTime = timestamp();
  double duration = lastSessionTime - firstSessionTime;
  if (duration > 0)
    avgAggregateThroughput = (unsigned int) (totalBytesRxed*8 / duration); 
  else{ 
    avgAggregateThroughput = 0;
    printf("\nperfServer:  WARNING: Terminating: Duration zero ??  (#msgs:%d)\n", receivedCount);
  }

  printf("\nperfServer:EXIT:(totalTime:%9.3f): #sessions:%d, AggThroughput:%d, #msgs:%d,repliedCount:%d,dropCount:%d\n", 
       endTime-startTime,sessionCount,avgAggregateThroughput, receivedCount, repliedCount,dropCount);

  // Link any remaining active to the archived
  if (firstSession == NULL)
    firstSession = firstActive;
  else
    lastSession->next = firstActive;

  printSessions();
  freeAllSessions();

  if (createDataFileFlag == 1 ) {
    fclose(newFile);
  }

  fflush(stdout);
  exit(0);
}
