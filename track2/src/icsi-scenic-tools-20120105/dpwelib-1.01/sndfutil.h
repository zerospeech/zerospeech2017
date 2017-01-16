/***************************************************************\
*	sndfutil.h						*
*	Headers for some handy extra soundfile utilities	*
*	02dec90	dpwe						*
\***************************************************************/

#include "dpwelib.h"
#include "sndf.h"

#define SFU_CHARMAX  	127
#define	SFU_CHARBITS	8
#define SFU_SHORTMAX 	32767
#define SFU_SHORTBITS 	16
#define SFU_LONGMAX 	2147483647
#define SFU_LONGBITS	32
/* Scaling for 32 bit integer samples = 24 bit samples plus 8 bit headroom */
#define SFU_24B_BITS	24
#define SFU_24B_MAX	8388607

int SFSampleSizeOf PARG((int format));
	/* return number of bytes per sample in given fmt */

void ConvertSampleBlockFormat PARG((char *inptr, char *outptr, 
				    int infmt, int outfmt, long samps));
int ConvertSoundFormat PARG((SFSTRUCT **psnd, int newFmt));

void ConvertChannels PARG((short *inBuf, short *outBuf, int inch, int ouch, 
			   long len));	/* # SAMPLE FRAMES to transfer */
void ConvertFlChans PARG((float *inBuf, float *outBuf, int inch, int ouch, 
			  long len));	/* # SAMPLE FRAMES to transfer */

void PrintSNDFinfo PARG((SFSTRUCT *theSound, char *name, char *tag, 
			 FILE *stream));
int PrintableStr PARG((char *s, int max));

long CvtChansAndFormat PARG((void *inbuf, void *outbuf, int inchans, 
			     int outchans, int infmt, int outfmt,long frames));
    /* convert the data format and the number of channels for a buffer 
       of sound.  Use smallest possible buffer space */

long ConvertChansAnyFmt PARG((void *inbuf, void *outbuf, int inchans, 
			      int outchans, int format, long frames));
    /* convert the number of channels in a sound of arbitrary format */


/* --- my_finfo fns --- */

FILE *my_fopen(char *name, char *mode);
void my_fclose(FILE *file);
int my_fread(void *buf, size_t size, size_t nitems, FILE *file);
int my_fwrite(void *buf, size_t size, size_t nitems, FILE *file);
int my_fpush(void *buf, size_t size, size_t nitems, FILE *file);
    /* push some data back onto a file so it will be read next;
       return the new pos of the file. */
void my_fsetpos(FILE *file, long pos);
    /* force the pos to be read as stated */
int my_fseek(FILE *file, long pos, int mode);
long my_ftell(FILE *file);
int my_fputs(char *s, FILE *file);
char *my_fgets(char *s, int n, FILE *stream);


