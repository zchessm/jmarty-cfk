/************************************************************************
* File:  common.h
*
* Purpose:
*   This include file is for common includes/defines.
*
* Notes:
*
* License and Copyright:  
*
************************************************************************/
#ifndef	__common_h
#define	__common_h



//Definition, FALSE is 0,  TRUE is anything other
#define TRUE 1
#define FALSE 0

#define VALID 1
#define NOTVALID 0

//Defines max size temp buffer that any object might create
#define MAX_TMP_BUFFER 1024

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

#endif


