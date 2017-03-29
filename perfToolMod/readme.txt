This has been co-opted by MattyK for testing purposes


Last update:  1/25/2017

The perfClient sends a stream of UDP packets to a server:port.
The perfServer receives packets and records which arrive and at what time.
The client/server can be told to operate in a 'unicast' mode or 'RTT' mode.   
Unicast mode means the server NEVER sends reply packets to the client.
RTT mode means the server echoes back each packet that arrives (actually, the server can be told how frequently to ack by settings the Ack Strategy parameter to something >1).
            
perfClient parameters:

The client sends UDP packets (from the application perspective,sends messages)
at a regular rate.  This tool is a traffic generator that models a constant bit rate sending pattern - although instead of transmitting at a true constant bit rate,  it transmit at a constant packet rate.

The client uses a token bucket to achieve the rate control. 
Tokens are generated at a constant rate (call it Tr).  Every 1/Tr seconds,
a token is generated.  The rate control runs each time a token is produced.  
The token is added to the token bucket. 
New data can be sent when there are tokens in the bucket.


./perfClient  localhost 5000 1000000 10000 1000 1000 0 100 129


*  char *  serverIP :server domain name or dotted decimal IP address = argv[1]; 
*  unsigned short  serverPort : server port 
*  uint  averageRate  - specifies the application sending rate
*  uint  bucketSize  - token bucket size (in bytes)
*  uint  tokenSize = - size of a token (bytes)
*  uint  messageSize - amount of application data on each socket send 
*                     
*  uint  mode =  0: RTT mode  ,   1: unicast mode
*         
*  uint  numberIterations :  number of client sends 
*  uint  debugFlag   used to set debugLevel
*
*    0:  nothing displayed except final result
*    1:  (default) adds errors and warning
*    2:  adds RTT samples
*    4:  some debug





perfServer parameters:
*
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





