/*
 *	$Id$
 *
 *	RATECONV.C
 *
 *	Convert sampling rate stdin to stdout
 *
 *	Copyright (c) 1992, 1995 by Markus Mummert
 *
 *	Redistribution and use of this software, modifcation and inclusion
 *	into other forms of software are permitted provided that the following
 *	conditions are met:
 *
 *	1. Redistributions of this software must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	2. If this software is redistributed in a modified condition
 *	   it must reveal clearly that it has been modified.
 *	
 *	THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 *	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *	PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 *	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 *	OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *	USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *	DAMAGE.
 *
 *
 *	history: 2.9.92		begin coding
 *		 5.9.92		fully operational
 *		 14.2.95 	provide BIG_ENDIAN, SWAPPED_BYTES_DEFAULT
 *				switches, Copyright note and References
 *		 25.11.95	changed XXX_ENDIAN to I_AM_XXX_ENDIAN
 *				default gain set to 0.8
 *		 3.12.95	stereo implementation
 *				SWAPPED_BYTES_DEFAULT now HBYTE1ST_DEFAULT
 *				changed [L/2] to (L-1)/2 for exact symmetry
 *
 *
 *	IMPLEMENTATION NOTES
 *
 *	Converting is achieved by interpolating the input samples in
 *	order to evaluate the represented continuous input slope at
 *	sample instances of the new rate (resampling). It is implemented 
 *	as a polyphase FIR-filtering process (see reference). The rate
 *	conversion factor is determined by a rational factor. Its
 *	nominator and denominator are integers of almost arbitrary
 *	value, limited only by coefficient memory size.
 *
 *	General rate conversion formula:
 *
 *	    out(n*Tout) = SUM in(m*Tin) * g((n*d/u-m)*Tin) * Tin
 *		      over all m
 *
 *	FIR-based rate conversion formula for polyphase processing:
 *
 *			  L-1
 *	    out(n*Tout) = SUM in(A(i,n)*Tin) * g(B(i,n)*Tin) * Tin
 *			  i=0
 *
 *	    A(i,n) = i - (L-1)/2 + [n*d/u]              
 *	           = i - (L-1)/2 + [(n%u)*d/u] + [n/u]*d 
 *	    B(i,n) = n*d/u - [n*d/u] + (L-1)/2 - i
 *	           =  ((n%u)*d/u)%1  + (L-1)/2 - i
 *	    Tout   = Tin * d/u
 *
 *	  where:
 *	    n,i		running integers
 *	    out(t)	output signal sampled at t=n*Tout
 *	    in(t)	input signal sampled in intervalls Tin
 *	    u,d		up- and downsampling factor, integers
 *	    g(t)	interpolating function
 *	    L		FIR-length of realized g(t), integer
 *	    /		float-division-operator
 *	    %		float-modulo-operator
 *	    []		integer-operator
 *
 *	  note:
 *	    (L-1)/2	in A(i,n) can be omitted as pure time shift yielding
 *			a causal design with a delay of ((L-1)/2)*Tin.
 *	    n%u		is a cyclic modulo-u counter clocked by out-rate
 *	    [n/u]*d	is a d-increment counter, advanced when n%u resets
 *	    B(i,n)*Tin	can take on L*u differnt values, at which g(t)
 *			has to be sampled as a coefficient array
 *
 *	Interpolation function design:
 *
 * 	    The interpolation function design is based on a sinc-function
 *	    windowed by a gaussian function. The former determines the
 *	    cutoff frequency, the latter limits the necessary FIR-length by
 *	    pushing the outer skirts of the resulting impulse response below
 *	    a certain threshold fast enough. The drawback is a smoothed
 *	    cutoff inducing some aliasing. Due to the symmetry of g(t) the
 *	    group delay of the filtering process is contant (linear phase).
 *
 *	    g(t) = 2*fgK*sinc(pi*2*fgK*t) * exp(-pi*(2*fgG*t)**2)
 *
 *	  where:
 *	    fgK		cutoff frequency of sinc function in f-domain
 *	    fgG		key frequency of gaussian window in f-domain
 *			reflecting the 6.82dB-down point
 *
 * 	  note:	    
 *	    Taking fsin=1/Tin as the input sampling frequncy, it turns out
 *	    that in conjunction with L, u and d only the ratios fgK/(fsin/2)
 *	    and fgG/(fsin/2) specify the whole proces. Requiring fsin, fgK
 *	    and fgG as input purposely keeps the notion of absolute
 *	    frequencies.
 *
 *	Numerical design:
 *
 *	    Samples are expected to be 16bit-signed integers, alternating
 *	    left and right channel in case of stereo mode- The byte order
 *	    per sample is selectable. FIR-filtering is implemented using
 *	    32bit-signed integer arithmetic. Coefficients are scaled to
 *	    find the output sample in the high word of accumulated FIR-sum.
 *
 *	    Interpolation can lead to sample magnitudes exceeding the
 *	    input maximum. Worst case is a full scale step function on the
 *	    input. In this case the sinc-function exhibits an overshoot of
 *	    2*9=18percent (Gibb's phaenomenon). In any case sample overflow
 *	    can be avoided by a gain of 0.8.
 *
 *	    If u=d=1 and if the input stream contains only a single sample,
 *	    the whole length of the FIR-filter will be written to the output.
 *	    In general the resulting output signal will be (L-1)*fsout/fsin
 *	    samples longer than the input signal. The effect is that a 
 *	    finite input sequence is viewed as padded with zeros before the
 *	    `beginning' and after the `end'. 
 *
 *	    The output lags ((L-1)/2)*Tin behind to implement g(t) as a
 *	    causal system corresponding to a causal relationship of the
 *	    discrete-time sequences in(m/fsin) and out(n/fsout) with
 *	    resepect to a sequence time origin at t=n*Tin=m*Tout=0.
 *
 *
 * 	REFERENCES
 *
 *	    Crochiere, R. E., Rabiner, L. R.: "Multirate Digital Signal
 *	    Processing", Prentice-Hall, Englewood Cliffs, New Jersey, 1983
 *
 *	    Zwicker, E., Fastl, H.: "Psychoacoustics - Facts and Models",
 *	    Springer-Verlag, Berlin, Heidelberg, New-York, Tokyo, 1990
 */

#include "stdafx.h"

/*
 *	adaptable defines and globals
 */
#define	BYTE		char		/* signed or unsigned */
#define	WORD		short		/* signed or unsigned, fit two BYTEs */
#define	LONG 		int		/* signed, fit two WORDs */

#ifndef MAXUP
#define	MAXUP		0x400		/* MAXUP*MAXLENGTH worst case malloc */
#endif

#ifndef MAXLENGTH
#define	MAXLENGTH	0x400		/* max FIR length */
#endif
					/* accounts for mono samples, means */
#define OUTBUFFSIZE 	(8*MAXLENGTH)	/* fit >=MAXLENGHT stereo samples */
#define INBUFFSIZE	(16*MAXLENGTH)	/* fit >=2*MAXLENGTH stereo samples */
#define sqr(a)	((a)*(a))
					/* platform architecture flags: */
#ifndef I_AM_BIG_ENDIAN			/*   adressing of high WORD of LONG */
# define I_AM_LITTLE_ENDIAN		/*   depends on this, as well as */
# define SWAP_BYTE_FLAG	-1		/*   the adressing of BYTE in WORD */	
#else
# define SWAP_BYTE_FLAG	0
#endif

#ifdef	HBYTE1ST_DEFAULT		/* HB,LB order in stream by default: */
 int	g_swapflag = SWAP_BYTE_FLAG;	/*   the magic about SWAP_BYTES_FLAG */
#else					/*   is to allow selection of byte */
 int	g_swapflag = !SWAP_BYTE_FLAG;	/*   order in stream independently */
#endif					/*   from platform architecture */

#ifdef	STEREO_DEFAULT
 int	g_monoflag = 0;
#else
 int	g_monoflag = -1;
#endif

/*
 *	other globals
 */
double	g_ampli = 0.8;			/* default gain, don't change */
int
	g_infilehandle = 0,		/* stdin */
	g_outfilehandle = 1,		/* stdout */
	g_firlen,			/* FIR-length */
	g_up,				/* upsampling factor */
	g_down				/* downsampling factor */
;

LONG
	g_sin[INBUFFSIZE],		/* input buffer */
	g_sout[OUTBUFFSIZE],		/* output buffer */
	*g_coep				/* coefficient array pointer */
;
double
	g_fsi,				/* input sampling frequency */
	g_fgk,				/* sinc-filter cutoff frequency */
	g_fgg				/* gaussian window key frequency */
;					/* (6.8dB down freq. in f-domain) */

/*
 *	evaluate sinc(x) = sin(x)/x safely
 */
double sinc(x)
double x;
{
	return(fabs(x) < 1E-50 ? 1.0 : sin(fmod(x,2*M_PI))/x);
}

/*
 *	evaluate interpolation function g(t) at t
 *	integral of g(t) over all times is expected to be one
 */
double interpol_func(t, fgk, fgg)
double t, fgk, fgg;
{
	return (2*fgk*sinc(M_PI*2*fgk*t)*exp(-M_PI*sqr(2*fgg*t)));
}

/*
 *	evaluate coefficient from i, q=n%u by sampling interpolation function 
 *	and scale it for integer multiplication used by FIR-filtering
 */
LONG coefficient(i, q, firlen, fgk, fgg, fsi, up, down, amp)
int i, firlen, q, up, down;
double fgk, fgg, fsi, amp;
{
	return(
	    (int)(0x10000 * amp *
		interpol_func(
		    (fmod(q*down/(double)up,1.) + (firlen-1)/2. - i) / fsi,
		    fgk,
		    fgg
		) / fsi
	    )
	);
}

/*
 *	I/O error handler, jumps to exit
 */
void ioerr_exit(p)
char *p;
{
	perror(p);
	free(g_coep);
	exit(-1);
}

/*
 *	transfer n LONGs from  s to d
 */
void transfer_int(s, d, n)
LONG *s, *d;
int n;
{
	LONG *e;

	if (n < 1)
		return;	
	e = d + n;
	while (d != e)
		*d++ = *s++;
}

/*
 *	zerofill n LONGs from s 
 */
void zerofill(s, n)
LONG *s;
int n;
{
	LONG *e;

	if (n < 1)
		return;	
	e = s + n;
	while (s != e)
		*s++ = 0;
}

/*
 *	convert buffer of n samples to LONGs
 */
void sample_to_int(buff, n)
WORD *buff;
int n;
{
	WORD *s, *e;
	LONG *d;

	if (n < 1)
		return;	
	s = buff + n;
	d = (LONG*)buff + n;
	e = buff;
	while (s != e) {
		*--d = (LONG)(*--s); 
	}
}

/*
 *	convert buffer of n LONGs to samples
 */
void int_to_sample(buff, n)
WORD *buff;
int n;
{
	WORD *s, *e, *d;

	if (n < 1)
		return;	
	s = buff;
	d = buff;
	e = buff + n*2;
	while (s != e) {

#ifndef I_AM_BIG_ENDIAN
		s++;
		*d++ = *s++; 
#else
		*d++ = *s++; 
		s++;
#endif
	}
}

/*
 *	swap bytes in buffer of n samples
 */
void byteswap_samples(buff, n)
BYTE *buff;
int n;
{
	BYTE *s, *d, *e;
	BYTE b;

	if (n < 1)
		return;	
	s = buff;
	d = buff;
	e = buff + n*2;
	while (s != e) {
		b = *s++;
		*d++ = *s++;
		*d++ = b;
	}
}

/*
 *	FIR-routines, mono and stereo
 *	this is where we need all the MIPS
 */
void fir_mono(inp, coep, firlen, outp)
register LONG *inp, *coep;
LONG *outp;
int firlen;
{
	register LONG akku = 0, *endp;
	int n1 = (firlen / 8) * 8, n0 = firlen % 8;

	endp = coep + n1;
	while (coep != endp) {
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
		akku += *inp++ * *coep++;
	}

	endp = coep + n0;
	while (coep != endp) {
		akku += *inp++ * *coep++;
	}
	*outp = akku;
}

void fir_stereo(inp, coep, firlen, out1p, out2p)
register LONG *inp, *coep;
LONG *out1p, *out2p;
int firlen;
{
	register LONG akku1 = 0, akku2 = 0, *endp;
	int n1 = (firlen / 8) * 8, n0 = firlen % 8;

	endp = coep + n1;
	while (coep != endp) {
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
	}

	endp = coep + n0;
	while (coep != endp) {
		akku1 += *inp++ * *coep;
		akku2 += *inp++ * *coep++;
	}
	*out1p = akku1;
	*out2p = akku2;
}

/*
 * 	filtering from input buffer to output buffer;
 *	returns number of processed samples in output buffer:
 *	if it is not equal to output buffer size,
 *	the input buffer is expected to be refilled upon entry, so that
 *	the last firlen numbers of the old input buffer are
 *	the first firlen numbers of the new input buffer;
 *	if it is equal to output buffer size, the output buffer
 *	is full and is expected to be stowed away;
 *
 */
int filtering_on_buffers
	(inp, insize, outp, outsize, coep, firlen, up, down, monoflag)
LONG *inp, *outp, *coep;
int insize, outsize, firlen, up, down, monoflag;
{
	static int inbaseidx = 0, inoffset = 0, cycctr = 0, outidx = 0;

	if (monoflag) {
		while (-1) {
			inoffset = (cycctr * down)/up;
			if ((inbaseidx + inoffset + firlen) > insize) {
				inbaseidx -= insize - firlen + 1;
				return(outidx);
			}
			fir_mono(inp + inoffset + inbaseidx,
				 coep + cycctr * firlen,
					firlen, outp + outidx++);
			cycctr++;
			if (!(cycctr %= up))
				inbaseidx += down;
			if (!(outidx %= outsize))
				return(outsize);
		}
	} else {
		/*
		 * rule how to convert mono routine to stereo routine:
		 * firlen, up, down and cycctr relate to samples in general,
		 * wether mono or stereo; inbaseidx, inoffset and outidx as
		 * well as insize and outsize still account for mono samples.
		 */
		while (-1) {
			inoffset = 2*((cycctr * down)/up);
			if ((inbaseidx + inoffset + 2*firlen) > insize) {
				inbaseidx -= insize - 2*firlen + 2;
				return(outidx);
			}
			fir_stereo(inp + inoffset + inbaseidx,
			 	coep + cycctr * firlen, firlen,
					outp + outidx++, outp + outidx++);
			cycctr++;
			if (!(cycctr %= up))
				inbaseidx += 2*down;
			if (!(outidx %= outsize))
				return(outsize);
		}
	}
}

/*
 *	read and convert input sample format to integer
 */
int intread(hd, buff, n)
void *hd;
unsigned int n;
LONG *buff;
{
	memcpy(buff, hd, n * sizeof(WORD));

	if (g_swapflag)
		byteswap_samples(buff, n);
	sample_to_int(buff,n);

	return n;
}

/*
 *	do some conversion jobs and write
 */
int intwrite(hd, buff, n)
void *hd;
unsigned int n;
LONG *buff;
{
	int_to_sample(buff, n);

	if (g_swapflag)
		byteswap_samples(buff, n);

	memcpy(hd, buff, n * sizeof(WORD));
	return n;
}

/*
 *	set up coefficient array
 */
void make_coe()
{
	int i, q;

	for (i = 0; i < g_firlen; i++) {
	    for (q = 0; q < g_up; q++) {
		g_coep[q * g_firlen + i] = coefficient(i, q, g_firlen,
		    g_fgk, g_fgg, g_fsi, g_up, g_down, g_ampli);
	    }
	}
}


int rateconv(WORD *in, unsigned int in_size, WORD *out, double fsin, double fgG, double fgK, int u, int d, int L, double gain, int stereo, int little_endian)
{
	unsigned int insize = 0, outsize = 0, skirtlen;
	unsigned int out_size = 0;

	g_firlen = L;
	g_fsi = fsin;
	g_fgk = fgK;
	g_fgg = fgG;
	g_up = u;
	g_down = d;
	g_ampli = gain;

	if (stereo)
		g_monoflag = 0;
	else
		g_monoflag = -1;

	if (little_endian)
		g_swapflag = !SWAP_BYTE_FLAG;
	else
		g_swapflag = SWAP_BYTE_FLAG;
	
	if ((g_coep = (LONG*)malloc(g_firlen * g_up * sizeof(int))) == NULL) 
		return 0;

	make_coe();
	skirtlen = (g_firlen - 1) * (g_monoflag ? 1 : 2);
	zerofill(g_sin, skirtlen);
	
	while (in_size > 0) 
	{
		if (INBUFFSIZE - skirtlen <= in_size)
			insize = intread(in, g_sin + skirtlen, INBUFFSIZE - skirtlen);
		else
			insize = intread(in, g_sin + skirtlen, in_size);

		in += insize;
		in_size -= insize;

	    do 
		{
			outsize = filtering_on_buffers(g_sin, skirtlen + insize, g_sout, OUTBUFFSIZE, g_coep, g_firlen, g_up, g_down, g_monoflag);
			if (outsize != OUTBUFFSIZE) 
			{
	    	    transfer_int(g_sin + insize, g_sin, skirtlen);
				break;
			}

			intwrite(out, g_sout, outsize);
			out += outsize;
			out_size += outsize;
	    } 
		while (-1);
 	}
	
	zerofill(g_sin + skirtlen, skirtlen);

	do 
	{
	    outsize = filtering_on_buffers(g_sin, skirtlen + skirtlen, g_sout, OUTBUFFSIZE, g_coep, g_firlen, g_up, g_down, g_monoflag);

		intwrite(out, g_sout, outsize);
		out += outsize;
		out_size += outsize;
	} 
	while (outsize == OUTBUFFSIZE); 

	free(g_coep);
	
	return out_size;
}

/*
 *	EOT
 */
