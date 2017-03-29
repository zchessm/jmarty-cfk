#include "perfTool.h"
#include <stdio.h>  /* for perror() */
#include <stdlib.h> /* for exit() */

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

char Version[] = "0.98";

void die(const char *msg) {
  if (errno == 0) {
    /* Just the specified message--no error code */
    puts(msg);
  } else {
    /* Message WITH error code/name */
    perror(msg);
  }
  printf("Die message: %s \n", msg);
  
  /* DIE */
  exit(EXIT_FAILURE);
}

long getMicroseconds(struct timeval *t) {
  return (t->tv_sec) * 1000000 + (t->tv_usec);
}

//Returns timeval in seconds with usecond precision
double convertTimeval(struct timeval *t) {
  return ( t->tv_sec + ( (double) t->tv_usec)/1000000 );
}

//Returns time difference in microseconds 
long getTimeSpan(struct timeval *start_time, struct timeval *end_time) {
  long usec2 = getMicroseconds(end_time);
  long usec1 = getMicroseconds(start_time);
  return (usec2 - usec1);
}

double timestamp() 
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) { 
    die("the sky is falling!"); 
  }
  return ((double)tv.tv_sec + ((double)tv.tv_usec / 1000000));
}

