
// Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/lib/msun/src/s_nextafterf.c,v 1.10 2005/03/07 04:55:58 das Exp $";
#endif

#include "math.h"
//#include "math_private.h"

typedef unsigned long int32_t;

#define GET_FLOAT_WORD(x, y) x = *(int32_t *) &(y)
#define SET_FLOAT_WORD(x, y)	\
do								\
{								\
	int32_t z = (y);			\
	x = *(float *) &(z);		\
} while(0)

float nextafterf(float x, float y)
{
        volatile float t;
        int32_t hx,hy,ix,iy;

        GET_FLOAT_WORD(hx,x);
        GET_FLOAT_WORD(hy,y);
        ix = hx&0x7fffffff;             /* |x| */
        iy = hy&0x7fffffff;             /* |y| */

        if((ix>0x7f800000) ||  		 /* x is nan */
           (iy>0x7f800000))     /* y is nan */
           return x+y;
        if(x==y) return y;              /* x=y, return y */
        if(ix==0) {                             /* x == 0 */
            SET_FLOAT_WORD(x,(hy&0x80000000)|1);/* return +-minsubnormal */
            t = x*x;
            if(t==x) return t; else return x;   /* raise underflow flag */
        }
        if(hx>=0) {                          /* x > 0 */
            if(hx>hy) {                      /* x > y, x -= ulp */
                hx -= 1;
            } else {                         /* x < y, x += ulp */
                hx += 1;
            }
        } else {                             /* x < 0 */
            if(hy>=0||hx>hy){                /* x < y, x -= ulp */
                hx -= 1;
            } else {                       	/* x > y, x += ulp */
                hx += 1;
            }
        }
        hy = hx&0x7f800000;
        if(hy>=0x7f800000) return x+x;  		/* overflow  */
        if(hy<0x00800000) {             		/* underflow */
            t = x*x;
            if(t!=x) {          			/* raise underflow flag */
                SET_FLOAT_WORD(y,hx);
                return y;
            }
        }
        SET_FLOAT_WORD(x,hx);
        return x;
}