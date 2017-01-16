/***********************************************\
*	ulaw.h					*
* Header file for mu-law conversion routines	*
* dpwe 14feb91					*
\***********************************************/

extern char 	exp_lut[];
extern short 	ulaw_decode[];

#ifdef __STDC__
void Short2Ulaw(short *sh_ptr, unsigned char *u_ptr, int sinc, int uinc,
		long len);
void Ulaw2Short(unsigned char *u_ptr, short *sh_ptr, int uinc, int sinc,
		long len);
void lin2alaw(short *linp, unsigned char *alawp, int linc, int ainc, long npts);
void alaw2lin(unsigned char *alawp, short *linp, int ainc, int linc, long npts);
#else /* not __STDC__ */
void Short2Ulaw();
void Ulaw2Short();
#endif /* __STDC__ */


