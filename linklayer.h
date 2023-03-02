/* Define a type called byte_t, if not already defined.
   This is an 8-bit variable, able to hold integers from 0 to 255.
   It could be named "byte", but this conflicts with a definition in
   windows.h, which is needed for the real physical layer functions. */
#ifndef BYTE_T_DEFINED
#define BYTE_T_DEFINED
typedef unsigned char byte_t; // define type "byte_t" for simplicity
#endif

#ifndef LINKLAYER_H_INCLUDED
#define LINKLAYER_H_INCLUDED

// Link Layer Protocol definitions - adjust all these to match your design
#define MAX_BLK 200   // largest number of data bytes allowed in one frame
#define OPT_BLK 70    // optimum number of data bytes in a frame
#define MOD_SEQNUM 16 // modulo for sequence numbers

// Frame marker byte values
#define STARTBYTE 212 // start of frame marker


// Frame header byte positions
#define SEQNUMPOS 2 // position of sequence number
#define FRAMENUMBERPOS 1 //position of frame size

// Header and trailer size
#define HEADERSIZE 3  // number of bytes in frame header
#define TRAILERSIZE 1 // number of bytes in frame trailer

// Frame error check results
#define FRAMEGOOD 1 // the frame has passed the tests
#define FRAMEBAD 0  // the frame is damaged

// Acknowledgement values
#define POSACK 1   // positive acknowledgement
#define NEGACK 26  // negative acknowledgement
#define ACK_SIZE 5 // number of bytes in ack frame

// Time limits
#define TX_WAIT 4.0 // sender waiting time in seconds
#define RX_WAIT 6.0 // receiver waiting time in seconds
#define MAX_TRIES 5 // number of times to re-try (either end)

// Physical Layer settings to be used
#define PORTNUM 1       // default port number: COM1
#define BIT_RATE 4800   // use a low speed for initial tests
#define PROB_ERR 0// probability of simulated error on receive

// Logical values
#define TRUE 1
#define FALSE 0

// Return codes - indicate success or what has gone wrong
#define SUCCESS 0   // good result, job done
#define BADUSE -9   // function cannot be used in this way
#define FAILURE -12 // function has failed for some reason
#define GIVEUP -15  // function has failed MAX_TRIES times

/* Functions to implement the link layer protocol.
   All functions take a debug argument - if non-zero, they print
   messages explaining what is happening.  Regardless of debug,
   functions print messages when things go wrong.
   Functions return negative values on failure.  */

/* Function to connect to another computer.
   Arguments:  portNum - port number to use, range 1 to 9,
               debugIn - controls printing of messages.
   Return value: 0 for success, negative for failure  */
int LL_connect(int portNum, int debugIn);

/* Function to disconnect from the other computer.
   It also prints a report of what happened while connected.
   Return value: 0 for success, negative for failure  */
int LL_discon(void);

/* Function to send a block of data in a frame - basic version.
   Arguments:  dataTX - pointer to array of data bytes to send,
               nTXdata - number of data bytes to send.
   Return value:  0 for success, negative for failure  */
int LL_send_basic(byte_t *dataTX, int nTXdata);

/* Function to send a block of data in a frame with full LLC protocol.
   Arguments:  dataTX - pointer to array of data bytes to send,
               nTXdata - number of data bytes to send.
   Return value:  0 for success, negative for failure  */
int LL_send_LLC(byte_t *dataTX, int nTXdata);

/* Function to receive a frame and return a block of data - basic version.
   Arguments:  dataRX - pointer to an array to hold the data block,
               maxData - maximum size of the data block.
   Return value: the size of the data block, or negative on failure.  */
int LL_receive_basic(byte_t *dataRX, int maxData);

/* Function to receive a frame and return a block of data with full LLC protocol.
   Arguments:  dataRX - pointer to an array to hold the data block,
               maxData - maximum size of the data block.
   Return value: the size of the data block, or negative on failure.  */
int LL_receive_LLC(byte_t *dataRX, int maxData);

/* Function to return the optimum size of a data block.
   This is currently specified as a constant in linklayer.h
   Return value: the optimum block size, in bytes.  */
int LL_getOptBlockSize(void);

// ==========================================================
// Functions called by the main link layer functions above

/* Function to build a frame around a block of data.
   Arguments: frameTX - pointer to an array to hold the frame,
              dataTX - array of data bytes to be put in the frame,
              nDataTX - number of data bytes to be put in the frame,
              seqNumTX - sequence number to include in the frame header.
   Return value: the total number of bytes in the frame.  */
int buildDataFrame(byte_t *frameTX, byte_t *dataTX, int nDataTX, int seqNumTX);

/* Function to find a frame and extract it from the bytes received.
   Arguments: frameRX - pointer to an array of bytes to hold the frame,
              maxSize - maximum number of bytes to receive,
              timeLimit - time limit for receiving a frame.
   Return value: the number of bytes in the frame, or zero if time limit
                 or size limit was reached before the frame was received,
                 or a negative value if there was some other problem. */
int getFrame(byte_t *frameRX, int maxSize, float timeLimit);

/* Function to check a received frame for errors.
   Arguments: frameRX - pointer to an array of bytes holding a frame,
              sizeFrame - number of bytes in the frame.
   Return value:  indicates if the frame is good or bad.  */
int checkFrame(byte_t *frameRX, int sizeFrame);

/* Function to process a received frame, to extract the data & sequence number.
   Arguments: frameRX - pointer to the array holding the frame,
              sizeFrame - number of bytes in the frame,
              dataRX - pointer to an array to hold the data bytes,
              maxData - maximum number of data bytes to extract,
              seqNumRX - pointer to the sequence number extracted.
   Return value: the number of data bytes extracted from the frame. */
int processFrame(byte_t *frameRX, int sizeFrame,
                 byte_t *dataRX, int maxData, int *seqNumRX);

/* Function to send an acknowledgement - positive or negative.
   Arguments: type - type of acknowledgement (POSACK or NEGACK),
              seqNum - sequence number that the ack should carry.
   Return value:  indicates success or failure.  */
int sendAck(int type, int seqNum);

// ==========================================================
// Helper functions used by various other functions

/* Function to advance the sequence number, modulo a constant
   specified in linklayer.h.
   Argument:   seq - the current sequence number.
   Return value: the new sequence number.  */
int next(int seq);

/* Function to set a time limit at a point in the future.
   Argument:   limit - the time limit in seconds, from now.
   Return value: the time at which the limit will elapse. */
long timeSet(float limit);

/* Function to check if a time limit has elapsed.
   Argument:  timeLimit - end time from timeSet() function.
   Return value: TRUE if the limit has elapsed, FALSE if not. */
int timeUp(long timeLimit);

/* Function to check if a byte is one of the protocol bytes.
   Argument:  b - byte value to check
   Return value: TRUE if b is a protocol byte, FALSE if not. */
int special(byte_t b);

/* Function to print the bytes of a frame, in groups of 10.
   Arguments:  frame - pointer to an array holding the frame,
               sizeFrame - the number of bytes in the frame.  */
void printFrame(byte_t *frame, int sizeFrame);

#endif // LINKLAYER_H_INCLUDED
