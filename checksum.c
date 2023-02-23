/*functions to implement the checksum*/

#include <stdio.h>
#include <stdlib.h>
#include "checksum.h"  //this will give the function prototypes for this
#include "linklayer.h" //I want to use #defines from here

static int debug = 1; //Used for printing out the checksum values at each step

byte_t makeCHKSUM(byte_t *dataTX, int nDataTX, byte_t frameSize, byte_t seqnum)
{
    //Construct the checksum to send
    byte_t checksum = (frameSize + seqnum); // checksum starts with a value equal to the sequence number
    for (int i = 0; i < nDataTX; i++)
    {
        // then we add every value from the data bits
        checksum += dataTX[i];
    }
    if (debug == 1)
    {
        //if we are in debug, print this return value
        printf("\nMADE CHECKSUM = %d\n", checksum % MODULO);
    }

    return (checksum % MODULO);
}
int inspectCHKSUM(byte_t *frameRX, int sizeFrame)
{
    // construct a new checksum and compare it with the value found in the recieved message
    byte_t checksum = 0;
    // we use bytes so that this checksum will never be larger than the 1 byte space allocated
    for (int c = 1; c < sizeFrame - TRAILERSIZE; c++)
    {
        //for each byte that is not the start bit, checksum or end bit
        checksum += frameRX[c];
        //add this to the checksum
    }
    if (debug == 1)
    {
        printf("\nSOLVED CHECKSUM = %d\n", checksum % MODULO);
        printf("\nFound CHECKSUM = %d\n", frameRX[sizeFrame - TRAILERSIZE]);
        //print the two different checksums
    }
    if (frameRX[sizeFrame - TRAILERSIZE] != (checksum % MODULO)) //if they dont match therefore bad frame
    {
        return (FRAMEBAD);
    }
    else
    {
        //otherwise good frame
        return (FRAMEGOOD);
    }
}
