/* Functions to implement a link layer protocol:
   LL_connect() connects to another computer;
   LL_discon()  disconnects;
   LL_send_basic()  sends a block of data, does not wait for a response;
   LL_send_LLC()    sends a block of data, using full LLC protocol;
   LL_receive_basic()  waits to receive a block of data;
   LL_receive_LLC()    tries to receive a block of data, sends a response;
   LL_getOptBlockSize()  returns the optimum size of data block
   All functions take a debug argument - if non-zero, they print
   messages explaining what is happening.  Regardless of debug,
   functions print messages when things go wrong.
   All functions return negative values on failure.
   Definitions of constants are in the header file.  */

#include <stdio.h>     // input-output library: print & file operations
#include <time.h>      // for timing functions
#include "physical.h"  // physical layer functions
#include "linklayer.h" // these functions
#include "checksum.h"  // the checksum functions

/* These variables need to retain their values between function calls, so they
   are declared as static.  By declaring them outside any function, they are
   made available to all the functions in this file.  */
static int seqNumTX;          // sequence number for the next transmit data block
static int lastSeqRX;         // sequence number of last good data block received
static int connected = FALSE; // keep track of state of connection
static int framesSent = 0;    // count of frames sent
static int acksSent = 0;      // count of ACKs sent
static int naksSent = 0;      // count of NAKs sent
static int acksRX = 0;        // count of ACKs received
static int naksRX = 0;        // count of NAKs received
static int badFrames = 0;     // count of bad frames received
static int goodFrames = 0;    // count of good frames received
static int timeouts = 0;      // count of timeouts
static long timerRX;          // time value for timeouts at receiver
static long connectTime;      // time when connection was established
static int debug = 1;         // debug value - controls printing

// ===========================================================================
/* Function to connect to another computer.
   It calls PHY_open() and reports any problem.
    It initialises sequence numbers, stored in shared variables.
   It also initialises counters and captures the time, for reporting purposes.
   Arguments:  portNum - port number to use, range 1 to 9,
               debugIn - controls printing of messages while connected.
   Return value: 0 for success, negative for failure  */
int LL_connect(int portNum, int debugIn)
{
    /* Try to connect using port number given, bit rate as in header file,
       always uses 8 data bits, no parity, fixed time limits.  */
    debugIn = 1;
    int status = PHY_open(portNum, BIT_RATE, 8, 0, 1000, 50, PROB_ERR);
    if (status == SUCCESS) // check if succeeded
    {
        connected = TRUE;               // record that we are connected
        debug = debugIn;                // set the debug variable for other functions to use
        seqNumTX = 0;                   // set the first sequence number for the sender
                                        /* The receiver keeps track of the last good data block received.  It increments
                                        this to get the sequence number of the data block that it is expecting to receive.
                                        At the start, the "last good" sequence number should be a value that could never
                                        arise, but also a value that will increment to 0, to match the first sequence
                                        number used by the sender.  */
        lastSeqRX = 2 * MOD_SEQNUM - 1; // equivalent to -1 under modulo rules
        framesSent = 0;                 // initialise all counters for this new connection
        acksSent = 0;
        naksSent = 0;
        acksRX = 0;
        naksRX = 0;
        badFrames = 0;
        goodFrames = 0;
        timeouts = 0;
        connectTime = clock(); // capture time when connection was established
        if (debug)
            printf("LL: Connected\n");
        return SUCCESS;
    }
    else // failed
    {
        connected = FALSE; // record that we are not connected
        printf("LL: Failed to connect on port %d, PHY_open returned %d\n",
               portNum, status);
        return -status; // return a negative value to indicate failure
    }
} // end of LL_connect

// ===========================================================================
/* Function to disconnect from the other computer.
   It calls PHY_close() and prints a report of what happened while connected.
   Return value: 0 for success, negative for failure.  */
int LL_discon(void)
{
    long elapsedTime = clock() - connectTime;               // measure time connected
    float connTime = ((float)elapsedTime) / CLOCKS_PER_SEC; // convert to seconds
    int status = PHY_close();                               // try to disconnect
    connected = FALSE;                                      // assume we are no longer connected
    if (status == SUCCESS)                                  // check if succeeded
    {
        /* Print the report, including all the counters, as
           we don't know if we were sending or receiving. */
        printf("\nLL: Disconnected after %.2f s.  Sent %d data frames\n",
               connTime, framesSent);
        printf("LL: Received %d good and %d bad frames, had %d timeouts\n",
               goodFrames, badFrames, timeouts);
        printf("LL: Sent %d ACKs and %d NAKs\n", acksSent, naksSent);
        printf("LL: Received %d ACKs and %d NAKs\n", acksRX, naksRX);
        return SUCCESS;
    }
    else // failed
    {
        printf("LL: Failed to disconnect, PHY_close returned %d\n", status);
        return -status; // return negative value to indicate failure
    }
} // end of LL_discon

// ===========================================================================
/* Function to send a block of data in a frame - basic version.
   If connected, it builds a frame, then sends the frame using PHY_send.
   If this succeeds, it advances the sequence number and returns.
   Arguments:  dataTX - pointer to array of data bytes to send,
               nTXdata - number of data bytes to send.
   Return value:  0 for success, negative for failure  */
int LL_send_basic(byte_t *dataTX, int nTXdata)
{
    static byte_t frameTX[3 * MAX_BLK]; // array large enough for frame
    int sizeTXframe = 0;                // size of frame being transmitted
    int numSent;                        // number of bytes sent by PHY_send

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLS: Attempt to send while not connected\n");
        return BADUSE; // problem code
    }

    // Then check if block size OK - set limit to suit your design
    if (nTXdata > MAX_BLK)
    {
        printf("LLS: Cannot send block of %d bytes, max block size %d\n",
               nTXdata, MAX_BLK);
        return BADUSE; // problem code
    }

    // Build the frame - sizeTXframe is the number of bytes in the frame
    sizeTXframe = buildDataFrame(frameTX, dataTX, nTXdata, seqNumTX);

    // Send the frame, then check for problems
    numSent = PHY_send(frameTX, sizeTXframe); // send frame bytes
    if (numSent != sizeTXframe)               // problem!
    {
        printf("LLS: Block %d, failed to send frame\n", seqNumTX);
        return FAILURE; // problem code
    }

    // If the frame has been sent, report success and move on
    framesSent++; // increment frame counter (for report)
    if (debug)
        printf("LLS: Sent frame of %d bytes, data block %d\n",
               sizeTXframe, seqNumTX);

    seqNumTX = next(seqNumTX); // increment the sequence number
    return SUCCESS;

} // end of LL_send_basic

// ===========================================================================
/* Function to send a block of data in a frame with full LLC protocol.
   Arguments:  dataTX - pointer to array of data bytes to send,
               nTXdata - number of data bytes to send.
   Return value:  0 for success, negative for failure  */
int LL_send_LLC(byte_t *dataTX, int nTXdata)
{
    static byte_t frameTX[3 * MAX_BLK]; // array large enough for frame
    static byte_t frameAck[ACK_SIZE];   // allow extra bytes for stuffing
    int sizeTXframe = 0;                // size of frame being transmitted
    int sizeAck = 0;                    // size of ACK frame received
    int seqAck;                         // sequence number in response received
    int attempts = 0;                   // number of attempts to send this data block
    int success = FALSE;                // flag to indicate block sent and ACKed
    int numSent;                        // number of bytes sent by PHY_send

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLS: Attempt to send while not connected\n");
        return BADUSE; // problem code
    }

    // Then check if block size OK - set limit to suit your design
    if (nTXdata > MAX_BLK)
    {
        printf("LLS: Cannot send block of %d bytes, max block size %d\n",
               nTXdata, MAX_BLK);
        return BADUSE; // problem code
    }

    // Build the frame - sizeTXframe is the number of bytes in the framcheckFramee
    sizeTXframe = buildDataFrame(frameTX, dataTX, nTXdata, seqNumTX);

    /* Then loop, sending the frame and waiting for a response.
       Repeat until success - a positive response is received.  */
    do
    {
        // Send the frame, then check for problems
        numSent = PHY_send(frameTX, sizeTXframe); // send frame bytes
        if (numSent != sizeTXframe)               // problem!
        {
            printf("LLS: Block %d, failed to send frame\n", seqNumTX);
            return FAILURE; // problem code
        }

        // The frame has been sent - update counters
        framesSent++; // increment frame counter (for report)
        attempts++;   // increment attempt counter, so we don't try forever
        if (debug)
            printf("LLS: Sent frame of %d bytes, block %d, attempt %d\n",
                   sizeTXframe, seqNumTX, attempts);

        // Now wait to receive a response (ack or nak)

        sizeAck = getFrame(frameAck, 2 * ACK_SIZE, 2 * TX_WAIT);
        if (sizeAck < 0)    // some problem receiving
            return FAILURE; // quit if failed

        else if (sizeAck == 0) // time limit reached
        {
            if (debug)
                printf("LLS: Timeout waiting for response\n");
            timeouts++; // increment counter for report
            /* What else should be done about that (if anything)?
               If success remains FALSE, this loop will continue, so
               it will re-transmit the frame and wait for a response... */
        }
        else // we have received a response frame - check it
        {
            if (debug)
                printf("LLS: Response received, size %d\n", sizeAck);
            // Check the frame, and extract some information
            if (checkFrame(frameAck, sizeAck) == FRAMEGOOD) // good frame
            {
                goodFrames++; // increment counter for report

                // Extract some information from the response
                seqAck = (int)frameAck[SEQNUMPOS]; // get sequence number
                // If there is more than one type of response, get the type

                /* Need to check if this is a positive ACK,
                   and if it relates to the data block just sent... */
                if ((frameAck[SEQNUMPOS + 1] == POSACK) && (seqAck == seqNumTX)) // need a sensible test here!!
                {
                    if (debug)
                        printf("LLS: ACK received, seq %d\n", seqAck);
                    acksRX++;       // increment counter for report
                    success = TRUE; // job is done
                }
                else // response could be NAK, or ACK for wrong block...
                {
                    if (debug)
                        printf("LLS: Response received, type %d, seq %d\n",
                               (int)frameAck[SEQNUMPOS + 1], seqAck); // need sensible values here!!
                    naksRX++;                                         // increment counter for report
                                                                      /* What else should be done about this (if anything)?
                                                                         If success remains FALSE, this loop will continue, so
                                                                         it will re-transmit the frame and wait for a response... */
                }
            }
            else // bad frame received - errors found
            {
                badFrames++; // increment counter for report
                if (debug)
                    printf("LLS: Bad frame received\n");
                // No point in trying to extract anything from a bad frame.
                /* What else should be done about this (if anything)?
                   If success remains FALSE, this loop will continue, so
                   it will re-transmit the frame and wait for a response... */
            }

        } // end of received frame processing

    } // repeat all this until succeed or reach the limit
    while ((success == FALSE) && (attempts < MAX_TRIES));

    if (success == TRUE) // the data block has been sent and acknowledged
    {
        seqNumTX = next(seqNumTX); // increment the sequence number
        return SUCCESS;
    }
    else // maximum number of attempts has been reached, without success
    {
        if (debug)
            printf("LLS: Block %d, tried %d times, failed\n",
                   seqNumTX, attempts);
        return GIVEUP; // tried enough times, giving up
    }
} // end of LL_send_LLC

// ===========================================================================
/* Function to receive a frame and extract a block of data - basic version.
   If connected, try to get a frame from the received bytes.
   If a frame is found, check if it is a good frame, with no errors.
   If good, extract the data and return.
   If bad, return a block of ten # characters instead of the data.
   Arguments:  dataRX - pointer to an array to hold the data block,
               maxData - maximum size of the data block.
   Return value: the size of the data block, or negative on failure.  */
int LL_receive_basic(byte_t *dataRX, int maxData)
{
    static byte_t frameRX[3 * MAX_BLK]; // create an array to hold the frame
    int nRXdata = 0;                    // number of data bytes received
    int sizeRXframe = 0;                // number of bytes in the frame received
    int seqNumRX = 0;                   // sequence number of the received frame
    int success = FALSE;                // flag to indicate success
    int attempts = 0;                   // attempt counter
    int i = 0;                          // used in for loop

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLR: Attempt to receive while not connected\n");
        return BADUSE; // problem code
    }

    // Loop to receive a frame, repeats until a frame is received.
    do
    {
        /* First get a frame, up to size of frame array, with time limit.
           getFrame function returns the number of bytes in the frame,
           or zero if it did not receive a frame within the time limit
           or a negative value if there was some other problem. */
        sizeRXframe = getFrame(frameRX, 3 * MAX_BLK, RX_WAIT);
        if (sizeRXframe < 0) // some problem receiving
            return FAILURE;  // quit if there was a problem

        attempts++;           // increment the attempt counter
        if (sizeRXframe == 0) // a timeout occurred
        {
            printf("LLR: Timeout trying to receive frame, attempt %d\n",
                   attempts);
            timeouts++; // increment the counter for the report
        }
        else // we have received a frame
        {
            if (debug)
                printf("LLR: Got frame, %d bytes, attempt %d\n",
                       sizeRXframe, attempts);

            // Now check the frame for errors
            if (checkFrame(frameRX, sizeRXframe) == FRAMEBAD) // frame is bad
            {
                badFrames++; // increment the bad frame counter
                if (debug)
                    printf("LLR: Bad frame received\n");
                // Put some dummy bytes in the data array
                for (i = 0; i < 10; i++)
                    dataRX[i] = 35; // # symbol
                nRXdata = 10;       // number of dummy bytes
                success = TRUE;     // pretend this is a success
            }
            else // we have a good frame - process it
            {
                goodFrames++; // increment the good frame counter
                // Extract the data bytes and the sequence number
                nRXdata = processFrame(frameRX, sizeRXframe, dataRX,
                                       maxData, &seqNumRX);
                if (debug)
                    printf("LLR: Received block %d with %d data bytes\n",
                           seqNumRX, nRXdata);
                success = TRUE; // this is regarded as success
                // the sequence number is ignored in this basic version

            } // end of good frame processing

        } // end of received frame processing

    } // repeat all this until succeed or reach the limit
    while ((success == FALSE) && (attempts < MAX_TRIES));

    if (success == TRUE) // received a frame
        return nRXdata;  // return number of data bytes extracted from frame

    else // failed to get a frame within the limit
    {
        if (debug)
            printf("LLR: Tried to receive a frame %d times, failed\n",
                   attempts);
        return GIVEUP; // tried enough times, giving up
    }

} // end of LL_receive_basic

// ===========================================================================
/* Function to receive a frame and extract a block of data, using LLC protocol.
   If connected, it tries to get a frame from the received bytes.  It keeps
   trying until it gets a good frame, with no errors and the expected sequence
   number, then it returns with the data bytes from the frame.
   Arguments:  dataRX - pointer to an array to hold the data block,
               maxData - maximum size of the data block.
   Return value: the size of the data block, or negative on failure.  */
int LL_receive_LLC(byte_t *dataRX, int maxData)
{
    static byte_t frameRX[3 * MAX_BLK]; // create an array to hold the frame
    int nRXdata = 0;                    // number of data bytes received
    int sizeRXframe = 0;                // number of bytes in the frame received
    int seqNumRX = 0;                   // sequence number of the received frame
    int success = FALSE;                // flag to indicate success
    int attempts = 0;                   // attempt counter
    int expected = next(lastSeqRX);     // calculate expected sequence number

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLR: Attempt to receive while not connected\n");
        return BADUSE; // problem code
    }

    /* Loop to receive a frame, repeats until a good frame with
       the expected sequence number is received. */
    do
    {
        /* First get a frame, up to size of frame array, with time limit.
           getFrame() returns the number of bytes in the frame,
           or zero if it did not receive a frame within the time limit
           or a negative value if there was some other problem.  */
        sizeRXframe = getFrame(frameRX, 3 * MAX_BLK, RX_WAIT);
        if (sizeRXframe < 0) // some problem receiving
            return FAILURE;  // quit if there was a problem

        attempts++;           // increment the attempt counter
        if (sizeRXframe == 0) // a timeout occurred
        {
            printf("LLR: Timeout trying to receive frame, attempt %d\n",
                   attempts);
            timeouts++; // increment the counter for the report
            /* No frame was received, so no response is needed.
               If success remains FALSE, the loop will continue
               and try again...  */
        }
        else // we have received a frame
        {
            if (debug)
                printf("LLR: Got frame, %d bytes, attempt %d\n",
                       sizeRXframe, attempts);

            // Now check the frame for errors
            if (checkFrame(frameRX, sizeRXframe) == FRAMEBAD) // frame is bad
            {
                badFrames++; // increment the bad frame counter
                if (debug)
                    printf("LLR: Bad frame received\n");
                /* What should the receiver do now?
                   Maybe send a response to the sender ?
                   If so, what sequence number ?
                   See the sendAck() function below.  */
                sendAck(NEGACK, seqNumRX); // send bad ack on badframe
            }
            else // we have a good frame - process it
            {
                goodFrames++; // increment the good frame counter
                // Extract the data bytes and the sequence number
                nRXdata = processFrame(frameRX, sizeRXframe, dataRX,
                                       maxData, &seqNumRX);
                if (debug)
                    printf("LLR: Received block %d with %d data bytes\n",
                           seqNumRX, nRXdata);

                // Check the sequence number - is this the data block we want?
                if (seqNumRX == expected) // got the expected data block
                {
                    success = TRUE;       // job is done
                    lastSeqRX = seqNumRX; // update last sequence number
                                          /* Maybe send a response to the sender ?
                                             If so, what sequence number ?
                                             See the sendAck() function below. */
                    sendAck(POSACK, seqNumRX); // send good ack otherwise
                }
                else if (seqNumRX == lastSeqRX) // got a duplicate data block
                {
                    if (debug)
                        printf("LLR: Duplicate rx seq. %d, expected %d\n",
                               seqNumRX, expected);
                    // What should be done about this?
                    sendAck(NEGACK, seqNumRX); // send bad ack on badframe
                }
                else // some other data block??
                {
                    if (debug)
                        printf("LLR: Unexpected block rx seq. %d, expected %d\n",
                               seqNumRX, expected);
                    // What should be done about this?
                    sendAck(NEGACK, seqNumRX); // send bad ack on badframe

                } // end of sequence number checking

            } // end of good frame processing

        } // end of received frame processing

    } // repeat all this until succeed or reach the limit
    while ((success == FALSE) && (attempts < MAX_TRIES));

    if (success == TRUE) // received good frame with expected sequence number
        return nRXdata;  // return number of data bytes extracted from frame
    else                 // failed to get a good frame within limit
    {
        if (debug)
            printf("LLR: Tried to receive a frame %d times, failed\n",
                   attempts);
        return GIVEUP; // tried enough times, giving up
    }

} // end of LL_receive_LLC

// ===========================================================================
/* Function to return the optimum size of a data block.
   This is currently specified as a constant in linklayer.h
   Return value: the optimum block size, in bytes.  */
int LL_getOptBlockSize(void)
{
    if (debug)
        printf("LLGOBS: Optimum size of data block is %d bytes\n",
               OPT_BLK);
    return OPT_BLK;
}

// ==========================================================
// Functions called by the main link layer functions above

// ===========================================================================
/* Function to build a frame around a block of data.
   This function puts the header bytes into the frame, then copies in the
   data bytes.  Then it adds the trailer bytes to the frame.
   It calculates the total number of bytes in the frame, and returns this
   value to the calling function.
   Arguments: frameTX - pointer to an array to hold the frame,
              dataTX - array of data bytes to be put in the frame,
              nDataTX - number of data bytes to be put in the frame,
              seqNumTX - sequence number to include in the frame header.
   Return value: the total number of bytes in the frame.  */
int buildDataFrame(byte_t *frameTX, byte_t *dataTX, int nDataTX, int seqNumTX)
{
    int i = 0; // for use in loop

    byte_t framesize = (byte_t)nDataTX + 2; // The framsize is equal to the number of data bytes + the seq. number + the checksum

    // Build the frame header first
    frameTX[0] = STARTBYTE; // start of frame marker bytec

    frameTX[FRAMENUMBERPOS] = framesize; // put the framesize byte in place

    frameTX[SEQNUMPOS] = (byte_t)seqNumTX; // sequence number as given

    // Copy the data bytes into the frame, starting after the header
    for (i = 0; i < nDataTX; i++) // step through the data array
    {
        frameTX[HEADERSIZE + i] = dataTX[i]; // copy each data byte
    }

    frameTX[HEADERSIZE + nDataTX] = makeCHKSUM(dataTX, nDataTX, framesize, (byte_t)seqNumTX);
    // create the checksum using the makeCHKSUM function, it takes arguments for the framesize, data bytes and sequence number value

    // Return the size of the frame
    return HEADERSIZE + nDataTX + TRAILERSIZE;
} // end of buildDataFrame

// ===========================================================================
/* Function to find a frame and extract it from the bytes received.
   Arguments: frameRX - pointer to an array of bytes to hold the frame,
              maxSize - maximum number of bytes to receive,
              timeLimit - time limit for receiving a frame.
   Return value: the number of bytes in the frame, or zero if time limit
                 or size limit was reached before the frame was received,
                 or a negative value if there was some other problem. */

int getFrame(byte_t *frameRX, int maxSize, float timeLimit)
{
    int bytesRX = 0;  // number of bytes received so far
    int bytesGot = 0; // return value from PHY_get()
    byte_t framesize = 0;

    timerRX = timeSet(timeLimit); // set time limit to wait for frame

    // First search for the start of frame marker
    do
    {
        bytesGot = PHY_get(frameRX, 1); // get one byte at a time
        // Return value is number of bytes received, or negative for problem
        if (bytesGot < 0)
            return bytesGot; // check for problem and give up
        else
            bytesRX += bytesGot; // otherwise update the bytes received count
    } while (((bytesGot < 1) || (frameRX[0] != STARTBYTE)) && !timeUp(timerRX));
    // until we get a byte which is a start of frame marker, or timeout

    // If we are out of time, without finding the start marker,
    // report the facts, but return 0 - no useful bytes received
    if (timeUp(timerRX))
    {
        printf("LLGF: Timeout seeking START, %d bytes received\n", bytesRX);
        return 0; // no frame received, but not a failure situation
    }

    bytesRX = 1; // if we found the start marker, we got 1 byte

    bytesGot = PHY_get((frameRX + bytesRX), 1);

    if (bytesGot < 0)
        return bytesGot; // check for problem and give up
    else
        bytesRX += bytesGot; // otherwise update the bytes received count

    framesize = frameRX[FRAMENUMBERPOS]; // get the framesize byte
    printf("\n FRAMESIZE :%d", framesize); // print the framesize byte
    bytesGot = PHY_get((frameRX + bytesRX), framesize); // get framesize bytes at a time
    if (bytesGot < 0)
        return bytesGot; // check for problem and give up
    else
        bytesRX += bytesGot; // otherwise update the bytes received count

    /* Now collect more bytes, until we find the end marker.
       If we had the frame size, we could do this more efficiently, by
       asking PHY_get() to collect all the remaining bytes in one call. */

    // If we filled the frame array, without finding the end marker,
    // this will be a bad frame, so so report the facts but return 0
    if (bytesRX >= maxSize)
    {
        printf("LLGF: Size limit seeking END, %d bytes received\n", bytesRX);
        return 0; // no frame received, but not a failure situation
    }

    // Otherwise, we found the end marker
    return bytesRX; // return the number of bytes in the frame
} // end of getFrame

// ===========================================================================
/* Function to check a received frame for errors.
   As a minimum, this function should check the error detecting code, but
   this example just checks the start and end markers.
   Arguments: frameRX - pointer to an array of bytes holding a frame,
              sizeFrame - number of bytes in the frame.
   Return value:  indicates if the frame is good or bad.  */
int checkFrame(byte_t *frameRX, int sizeFrame)
{
    int frameStatus = FRAMEGOOD; // initial frame status

    // Check the start-of-frame marker in the first byte
    if (frameRX[0] != STARTBYTE)
    {
        printf("LLCF: Frame bad - no start marker\n");
        frameStatus = FRAMEBAD;
    }

    // inspect the checksum
    frameStatus = inspectCHKSUM(frameRX, sizeFrame);

    // In debug mode, if frame is bad, print start and end bytes
    if (debug && (frameStatus == FRAMEBAD))
        printFrame(frameRX, sizeFrame);

    // Return the frame status
    return frameStatus;
} // end of checkFrame

// ===========================================================================
/* Function to process a received frame, to extract the data & sequence number.
   The frame has already been checked for errors, so this simple
   implementation assumes everything is where is should be.
   Arguments: frameRX - pointer to the array holding the frame,
              sizeFrame - number of bytes in the frame,
              dataRX - pointer to an array to hold the data bytes,
              maxData - maximum number of data bytes to extract,
              seqNumRX - pointer to the sequence number extracted.
   Return value: the number of data bytes extracted from the frame. */
int processFrame(byte_t *frameRX, int sizeFrame,
                 byte_t *dataRX, int maxData, int *seqNumRX)
{
    int i = 0;   // for use in loop
    int nRXdata; // number of data bytes in the frame

    // First get the sequence number from its place in the header
    *seqNumRX = (int)frameRX[SEQNUMPOS];

    // Calculate the number of data bytes, based on the frame size
    nRXdata = sizeFrame - HEADERSIZE - TRAILERSIZE;

    // Check if the number of data bytes is within the limit given
    if (nRXdata > maxData)
        nRXdata = maxData; // limit to the max allowed

    // Now copy the data bytes from the middle of the frame
    for (i = 0; i < nRXdata; i++)
    {
        dataRX[i] = frameRX[HEADERSIZE + i]; // copy one byte
    }

    return nRXdata; // return the size of the data block extracted
} // end of processFrame

// ===========================================================================
/* Function to send an acknowledgement - positive or negative.
   Arguments: type - type of acknowledgement (POSACK or NEGACK),
              seqNum - sequence number that the ack should carry.
   Return value:  indicates success or failure.
   Note type is used to update statistics for the report, so the argument
   is needed even if its value is not included in the ack frame. */
int sendAck(int type, int seqNum)
{
    byte_t ackFrame[ACK_SIZE + 2]; // allow extra bytes for byte stuff
    int sizeAck = ACK_SIZE;        // number of bytes in the ack frame so far
    int retVal;                    // return value from functions

    // First build the frame
    ackFrame[0] = STARTBYTE; 
    ackFrame[FRAMENUMBERPOS] = (ACK_SIZE - 1);
    ackFrame[SEQNUMPOS] = seqNum;
    switch (type)
    {
    case POSACK:
        ackFrame[SEQNUMPOS + 1] = FRAMEGOOD;
        break;

    default:
        ackFrame[SEQNUMPOS + 1] = FRAMEBAD;
        break;
    }
    byte_t *dataptr = &ackFrame[SEQNUMPOS + 1];
    ackFrame[ACK_SIZE - 1] = makeCHKSUM(dataptr, 1, (byte_t)(ACK_SIZE - 1), (byte_t)seqNum);
    if(debug)
        printf("ACKFRAME : %s, SIZEACK : %d", ackFrame, sizeAck);

    // Add more bytes to the frame, and update sizeAck

    // Then send the frame and check for problems
    retVal = PHY_send(ackFrame, sizeAck); // send the frame
    if (retVal != sizeAck)                // problem!
    {
        printf("LLSA: Failed to send response, seq. %d\n", seqNum);
        return FAILURE; // problem code
    }
    else // success - update the counters for the report
    {
        if (type == POSACK)
            acksSent++;
        else if (type == NEGACK)
            naksSent++;
        if (debug)
            // Print a message to show the frame sent
            printf("LLSA: Sent response of %d bytes, type %d, seq %d\n",
                   sizeAck, type, seqNum);
        return SUCCESS;
    }
} // end of sendAck

// ==========================================================
// Helper functions used by various other functions

// ===========================================================================
/* Function to advance the sequence number, modulo a constant
   specified in linklayer.h.
   Argument:   seq - the current sequence number.
   Return value: the new sequence number.  */
int next(int seq)
{
    return ((seq + 1) % MOD_SEQNUM);
}

// ===========================================================================
/* Function to set a time limit at a point in the future.
   Argument:   limit - the time limit in seconds, from now.
   Return value: the time at which the limit will elapse. */
long timeSet(float limit)
{
    long timeLimit = clock() + (long)(limit * CLOCKS_PER_SEC);
    return timeLimit;
} // end of timeSet

// ===========================================================================
/* Function to check if a time limit has elapsed.
   Argument:  timeLimit - end time from timeSet() function.
   Return value: TRUE if the limit has elapsed, FALSE if not. */
int timeUp(long timeLimit)
{
    if (clock() < timeLimit)
        return FALSE; // still within limit
    else
        return TRUE; // time limit has been reached or exceeded
} // end of timeUp

// ===========================================================================
/* Function to check if a byte is one of the protocol bytes.
   Argument:  b - a byte value to check
   Return value: TRUE if b is a protocol byte, FALSE if not.*/
int special(byte_t b)
{

    return FALSE; // no checking in this version
}

// ===========================================================================
/* Function to print the bytes of a frame, in groups of 8.
   For small frames, print all the bytes,
   for larger frames, just the start and end bytes.
   Arguments:  frame - pointer to an array holding the frame,
               sizeFrame - the number of bytes in the frame.  */
void printFrame(byte_t *frame, int sizeFrame)
{
    int i, j;

    if (sizeFrame <= 40) // small frame - print all the bytes
    {
        for (i = 0; i < sizeFrame; i += 8) // step in groups of 8 bytes
        {
            for (j = 0; (j < 8) && (i + j < sizeFrame); j++)
            {
                printf("%3d ", frame[i + j]); // print as number
            }
            printf(": "); // separator
            for (j = 0; (j < 8) && (i + j < sizeFrame); j++)
            {
                printf("%c", frame[i + j]); // print as character
            }
            printf("\n"); // new line
        }                 // end for
    }
    else // large frame - print start and end
    {
        for (j = 0; (j < 8); j++)     // first 8 bytes
            printf("%3d ", frame[j]); // print as number
        printf(": ");                 // separator
        for (j = 0; (j < 8); j++)
            printf("%c", frame[j]);                   // print as character
        printf("\n - - -\n");                         // new line, separator
        for (j = sizeFrame - 8; (j < sizeFrame); j++) // last 8 bytes
            printf("%3d ", frame[j]);                 // print as number
        printf(": ");                                 // separator
        for (j = sizeFrame - 8; (j < sizeFrame); j++)
            printf("%c", frame[j]); // print as character
        printf("\n");               // new line
    }

} // end of printFrame
