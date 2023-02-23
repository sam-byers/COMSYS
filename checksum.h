#ifndef BYTE_T_DEFINED
#define BYTE_T_DEFINED
typedef unsigned char byte_t;  // define type "byte_t" for simplicity
#endif

#ifndef CHECKSUM_H_INCLUDED
#define CHECKSUM_H_INCLUDED

#define MODULO 250    // define modulo value

int inspectCHKSUM(byte_t *frameRX, int sizeFrame);
byte_t makeCHKSUM(byte_t *dataTX, int nDataTX, byte_t frameSize, byte_t seqnum);
//fuction prototypes for the checksum

#endif
