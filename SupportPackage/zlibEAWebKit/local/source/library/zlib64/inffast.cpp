/* inffast.c -- process literals and length/distance pairs fast
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

#include "zutil.h"
#include "inftrees.h"
#include "infblock.h"
#include "inffast.h"
#include "infcodes.h"
#include "infutil.h"
#include "inflate.h"

struct inflate_codes_state {int dummy;}; /* for buggy compilers */

/* simplify the use of the inflate_huft type with some defines */
#define exop word.what.Exop
#define bits word.what.Bits

/* macros for bit input with no checking and for returning unused bytes */
#define GRABBITS(j) {while(k<(j)){b|=((uLong)NEXTBYTE)<<k;k+=8;}}
#define UNGRAB {c=z->avail_in-n;c=(k>>3)<c?k>>3:c;n+=c;p-=c;k-=c<<3;}

/* Called with number of bytes left to write in window at least 258
   (the maximum string length) and number of input bytes available
   at least ten.  The ten bytes are six bytes for the longest length/
   distance pair plus four bytes for overloading the bit buffer. */

int inflate_fast(
uInt bl, uInt bd,
inflate_huft *tl,
inflate_huft *td, /* need separate declaration for Borland C++ */
inflate_blocks_statef *s,
z_streamp z)
{
  inflate_huft *t;      /* temporary pointer */
  uInt maxlen;          /* maximum string length */
  uInt e;               /* extra bits or operation */
  uLong b;              /* bit buffer */
  uInt k;               /* bits in bit buffer */
  Bytef *p;             /* input data pointer */
  uInt n;               /* bytes available there */
  Bytef *q;             /* output window write pointer */
  uInt m;               /* bytes to end of window or read pointer */
  uInt ml;              /* mask for literal/length tree */
  uInt md;              /* mask for distance tree */
  uInt c;               /* bytes to copy */
  uInt d;               /* distance back to copy from */
  Bytef *r;             /* copy source pointer */

  /* load input, output, bit values */
  LOAD

  /* initialize masks */
  ml = inflate_mask[bl];
  md = inflate_mask[bd];

  maxlen = 258;
  if (z->state->method == Z_DEFLATED64)
    maxlen = 65535;

  /* do until not enough input or output space for fast loop */
  do {                          /* assume called with m >= maxlen && n >= 10 */
    /* get literal/length code */
    GRABBITS(20)                /* max bits for literal/length code */
    if ((e = (t = tl + ((uInt)b & ml))->exop) == 0)
    {
      DUMPBITS(t->bits)
      Tracevv((stderr, t->base >= 0x20 && t->base < 0x7f ?
                "inflate:         * literal '%c'\n" :
                "inflate:         * literal 0x%02x\n", t->base));
      *q++ = (Byte)t->base;
      m--;
      continue;
    }
    do {
      DUMPBITS(t->bits)
      if (e & 32)
      {
        /* get extra bits for length */
        e &= 31;
        c = t->base + ((uInt)b & inflate_mask[e]);

        // Case 3015: With ZLIB, e is worth 192 on the final loop, so it ends up
        //            in the last else block below. But with ZLIB64, e is worth 160.
        //            It is a normal condition to be different than with ZLIB, but I
        //            believe it should have been worth 128 instead of 160 (128+32).
        //       Ref: ZLIB: Can't have 16. Must have 64. Can't have 32.
        //            ZLIB64: Can't have 32. Must have one of 32, 64 or 128. Can't have 64.
        //  Solution: We error out if c is set to zero.
        if( c == 0 )
        {
          z->msg = (char*)"invalid literal/length code";
          UNGRAB
          UPDATE
          return Z_DATA_ERROR;
        }

        DUMPBITS(e)
        Tracevv((stderr, "inflate:         * length %u\n", c));

        /* decode distance base of block to copy */
        GRABBITS(15);           /* max bits for distance code */
        e = (t = td + ((uInt)b & md))->exop;
        do {
          DUMPBITS(t->bits)
          if (e & 32)
          {
            /* get extra bits to add to distance base */
            e &= 31;
            GRABBITS(e)         /* get extra bits (up to 13) */
            d = t->base + ((uInt)b & inflate_mask[e]);
            DUMPBITS(e)
            Tracevv((stderr, "inflate:         * distance %u\n", d));

            /* do the copy */
            m -= c;
            r = q - d;
            if (r < s->window)                  /* wrap if needed */
            {
              do {
                r += s->end - s->window;        /* force pointer in window */
              } while (r < s->window);          /* covers invalid distances */
              e = s->end - r;
              if (c > e)
              {
                c -= e;                         /* wrapped copy */
                do {
                    *q++ = *r++;
                } while (--e);
                r = s->window;
                do {
                    *q++ = *r++;
                } while (--c);
              }
              else                              /* normal copy */
              {
                *q++ = *r++;  c--;
                *q++ = *r++;  c--;
                do {
                    *q++ = *r++;
                } while (--c);
              }
            }
            else                                /* normal copy */
            {
              *q++ = *r++;  c--;
              *q++ = *r++;  c--;
              do {
                *q++ = *r++;
              } while (--c);
            }
            break;
          }
          else if (!(e & (32+64+128)))  /* next table */
          {
            t += t->base;
            e = (t += ((uInt)b & inflate_mask[e]))->exop;
          }
          else
          {
            z->msg = (char*)"invalid distance code";
            UNGRAB
            UPDATE
            return Z_DATA_ERROR;
          }
        } while (1);
        break;
      }
      if (!(e & (32+64+128)))  /* next table */
      {
        t += t->base;
        if ((e = (t += ((uInt)b & inflate_mask[e]))->exop) == 0)
        {
          DUMPBITS(t->bits)
          Tracevv((stderr, t->base >= 0x20 && t->base < 0x7f ?
                    "inflate:         * literal '%c'\n" :
                    "inflate:         * literal 0x%02x\n", t->base));
          *q++ = (Byte)t->base;
          m--;
          break;
        }
      }
      else if (e & 64)
      {
        Tracevv((stderr, "inflate:         * end of block\n"));
        UNGRAB
        UPDATE
        return Z_STREAM_END;
      }
      else
      {
        z->msg = (char*)"invalid literal/length code";
        UNGRAB
        UPDATE
        return Z_DATA_ERROR;
      }
    } while (1);
  } while (m >= maxlen && n >= 10);

  /* not enough input or output--restore pointers and return */
  UNGRAB
  UPDATE
  return Z_OK;
}
