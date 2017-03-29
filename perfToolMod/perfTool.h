/*********************************************************
*
* Module Name: Perf tool lient/server header file
*
* File Name:    perfTool.h
*
* Summary:
*  This file contains common stuff for the client and server
*
* Revisions:
*
*********************************************************/
#ifndef PERF_TOOL_H
#define PERF_TOOL_H

#include <arpa/inet.h> /* for sockaddr_in and inet_addr() */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h> /* for in_addr */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h> /* for close() */

#define MESSAGEMIN 24
#define MESSAGEMAX 65535

#ifndef LINUX
#define INADDR_NONE 0xffffffff
#endif

//Definition, FALSE is 0,  TRUE is anything other
#define TRUE 1
#define FALSE 0

#define VALID 1
#define NOTVALID 0

//Defines max size temp buffer that any object might create
#define MAX_TMP_BUFFER 1024


//The possible tool modes of operation  
#define PING_MODE 0      //server ack's each arrival (subject to its AckStrategy param)
#define LIMITED_RTT 1    //server ack's each arrival (subject to its AckStrategy param)
                         // client only maintains a single RTT sample active at a time
#define ONE_WAY_MODE  2  // server does not issue an ack

/*
  Consistent with C++, use EXIT_SUCCESS, SUCCESS is a 0, otherwise  EXIT_FAILURE  is not 0

  If the method returns a valid rc, EXIT_FAILURE is <0
   Should use bool when the method has a context appropriate for returning T/F.   
*/
#define SUCCESS 0
#define NOERROR 0

#define ERROR   -1
#define FAILURE -1
#define FAILED -1

//We assume a NULL can be interpretted as an error

void die(const char *msg);
void DieWithError(char *errorMessage); /* External error handling function */
double timestamp();
extern char Version[];

long getMicroseconds(struct timeval *t);
double convertTimeval(struct timeval *t);

long getTimeSpan(struct timeval *start_time, struct timeval *end_time);

typedef struct {
  uint16_t size;
  uint16_t mode;
  time_t timestamp;
  uint32_t sequenceNum;
  struct timeval timeSent;
  uint64_t timeSeconds;
  uint64_t microSeconds;
} messageHeaderLarge;

typedef struct {
  uint16_t size;
  uint16_t mode;
  uint32_t sequenceNum;
  uint32_t timeSentSeconds;
  uint32_t timeSentUSeconds;
  uint32_t timeRxSeconds;
  uint32_t timeRxUSeconds;
  uint32_t OWDelay;         //the sender fills this in if it can estimate the one way latency 
} messageHeader;



#endif

