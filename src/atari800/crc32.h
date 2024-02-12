#ifndef CRC32_H_
#define CRC32_H_

#include "ff.h"

#include "atari.h"

/* Compute CRC32 of LEN bytes from BUF. CRC should be set initially to
   0xffffffff. */
ULONG CRC32_Update(ULONG crc, UBYTE const *buf, unsigned int len);

/* Compute CRC32 of a stream F and store it at *RESULT. Return non-zero on
   success or 0 on read error. */
int CRC32_FromFile(FIL *f, ULONG *result);

#endif /* CRC32_H_ */
