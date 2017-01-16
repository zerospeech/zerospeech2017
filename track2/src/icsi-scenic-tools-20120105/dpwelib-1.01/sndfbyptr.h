/*
 * sndffnptr.h
 * 
 * Header file to change the 'standard' sndf*.c include file
 * into a set of fnptr definitions.
 * - trying to make sndf into a run-time adaptation
 *
 * 1997jan29 dpwe@icsi.berkeley.edu
 */

/* Define FNPREFIX before entry as the unique prefix for this type */

#ifndef SNDFNAMER
#  define ___cat(ARG1,ARG2)	ARG1 ## ARG2
#  define ___CAT(ARG1,ARG2)	___cat(ARG1,ARG2)
#else
#  undef SNDFNAMER
#endif /* !SNDFNAMER */

#define SNDFNAMER(REST)  ___CAT(FNPREFIX,REST)

#ifdef sndfExtn
/* remove previous definitions */
#undef sndfExtn
#undef SFReadHdr
#undef SFRdXInfo
#undef SFWriteHdr
#undef SFHeDaLen
#undef SFTrailerLen
#undef SFLastSampTell
#undef FixupSamples
#endif /* sndfExtn */

#define sndfExtn 	SNDFNAMER(sndfExtn)
#define SFReadHdr	SNDFNAMER(readHdr)
#define SFRdXInfo	SNDFNAMER(readXInfo)
#define SFWriteHdr	SNDFNAMER(writeHdr)
#define SFHeDaLen	SNDFNAMER(hdrLen)
#define SFTrailerLen	SNDFNAMER(trlrLen)
#define SFLastSampTell	SNDFNAMER(lastTell)
#define FixupSamples	SNDFNAMER(fixSamps)

