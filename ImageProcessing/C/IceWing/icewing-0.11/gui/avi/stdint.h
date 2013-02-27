/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

#ifndef iw_stdint_H
#define iw_stdint_H

#ifdef __alpha

typedef signed char    int8_t;
typedef unsigned char  uint8_t;
typedef short          int16_t;
typedef unsigned short uint16_t;
typedef int            int32_t;
typedef unsigned int   uint32_t;
typedef long           int64_t;
typedef unsigned long  uint64_t;

#else

#include <inttypes.h>

#endif

#endif /* iw_stdint_H */
