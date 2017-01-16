/*****************************************************************************\
*   ROUTINES IN THIS FILE:                                                    *
*                                                                             *
*     comp_Jah() :   For J-Rasta with adaptive J, comp_Jah is a routine for   *
*                    quantizing the J value                                   *
*     read_map_file: read in the mapping coefficients file  (default is       *
*                    map_weights.dat)                                         *
*     comp_Jboundaries: take the geometric mean between two j values as the   *
*                    decision threshold                                       *
*     quantize_jah:  quantize the j value                                     *
*     cr_map_source_data, do_mapping : Perform the spectral mapping           *
******************************************************************************/

/***********************************************************************

 (C) 1995 US West and International Computer Science Institute,
     All rights reserved
 U.S. Patent Numbers 5,450,522 and 5,537,647

 Embodiments of this technology are covered by U.S. Patent
 Nos. 5,450,522 and 5,537,647.  A limited license for internal
 research and development use only is hereby granted.  All other
 rights, including all commercial exploitation, are retained unless a
 license has been granted by either US West or International Computer
 Science Institute.

***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rasta.h"


/* local function prototypes */

void read_map_file(const struct param *pptr, struct map_param *mptr);
void comp_Jboundaries(struct map_param *mptr);
void quantize_jah(struct param *pptr,const struct map_param *mptr, int *mapset);

void comp_Jah(struct param *pptr, struct map_param *mptr, int *mapset)
{
     static int first_call = TRUE;


     char *funcname;
  
     funcname = "comp_Jah";

     if (first_call == TRUE)
     {
        read_map_file(pptr, mptr);
        comp_Jboundaries(mptr); 
        first_call = FALSE; 
     }
     quantize_jah(pptr, mptr, mapset); 
}   



void read_map_file(const struct param *pptr, struct map_param *mptr)
{
     FILE *map_file_fd;
     int  i, cr, j;
     char *funcname;

     funcname = "read_map_file";

     if (pptr->mapcoef_fname == NULL)
       {
         fprintf(stderr,
                 "Must provide a name for the J-RASTA mapping coefficients file.\n");
         exit(1);
       }

     if((map_file_fd = fopen(pptr->mapcoef_fname,"rt")) == NULL)
     {
        fprintf(stderr,"Cannot open the J-RASTA mapping coefficients file\n");
        exit(-1);
     }
     fscanf(map_file_fd,"%d", &(mptr->n_sets)); /* For default, n_sets is 7 */ 
     fscanf(map_file_fd,"%d", &(mptr->n_bands)); /* For default, n_bands is 15 since there are
                                                    17 critical bands and 15 are good */
     fscanf(map_file_fd,"%d", &(mptr->n_coefs)); /* For default, n_coefs is 16 */
     if (mptr->n_sets > MAXNJAH )
     {
        fprintf(stderr,"Number of mapping sets: %d not OK\n",mptr->n_sets);
        exit(-1);
     }
     if ((mptr->n_bands > MAXMAPBAND) || (mptr->n_bands != (pptr->nfilts - 2*(pptr->first_good) )))
     {
        fprintf(stderr,"Number of critical bands for mapping: %d not OK\n", mptr->n_bands);
        exit(-1);
     }
     if (mptr->n_coefs > MAXMAPCOEF)
     {
        fprintf(stderr,"Number of mapping coefficients/band: %d not OK\n", mptr->n_coefs);
        exit(-1);
     }
     for (i=0; i<mptr->n_sets; i++) 
     {
        fscanf(map_file_fd,"%e", &(mptr->jah_set[i]));
        for ( cr=0; cr< mptr->n_bands; cr++)
        { 
            for(j= 0; j < mptr->n_coefs; j++)
            {
               if (fscanf(map_file_fd,"%f",&(mptr->reg_coef[i][cr][j])) != 1)
               {
                  fprintf(stderr,"error reading map weights\n");
                  exit(-1);
               }
            }
        }
     }
     fclose(map_file_fd);
}
          

void comp_Jboundaries(struct map_param *mptr)
{ 
   int i;
   char *funcname;

   funcname = "comp_Jboundaries";
  
   for (i=0; i<mptr->n_sets-1 ; i++)
   {
     mptr->boundaries[i] = (float)sqrt((double)mptr->jah_set[i] * (double)mptr->jah_set[i+1]);
   }
} 


void quantize_jah(struct param *pptr,const struct map_param *mptr, int *mapset)
{
   int i;
   char *funcname;
  
   funcname = "quantize_jah";
 
   if ( pptr->jah > mptr->boundaries[0])
   {
      pptr->jah = mptr->jah_set[0];
      *mapset = 0;
   }
   i= 0;
   while (i < mptr->n_sets -2)
   {
      if (( pptr->jah < mptr->boundaries[i] ) && (pptr->jah > mptr->boundaries[i+1]))
      {
         pptr->jah = mptr->jah_set[i+1];
         *mapset = i+1;
      }
      i++;
   }
   if (pptr->jah < mptr->boundaries[mptr->n_sets -2])
   {
      pptr->jah = mptr->jah_set[mptr->n_sets -1];
      *mapset = mptr->n_sets - 1;
   }
}   
                     


struct fmat *cr_map_source_data(const struct param *pptr,const struct map_param *mptr,const struct fvec *ras_nl_aspectrum)
{
    char *funcname;
    static struct fmat *map_source_ptr = NULL;
    int i, j;

    funcname  = "cr_map_source_data";
    if (map_source_ptr == (struct fmat *)NULL)
    {
        map_source_ptr = alloc_fmat(mptr->n_bands, mptr->n_coefs);
    }
    /* The following depends entirely on the method of mapping */
    for (i=0; i<mptr->n_bands; i++)
    {
       for ( j = 0; j< (mptr->n_coefs -1); j++)
       {
           map_source_ptr->values[i][j] = ras_nl_aspectrum->values[pptr->first_good + j];
       }
       map_source_ptr->values[i][mptr->n_coefs -1] = 1;
    } 
    return(map_source_ptr);
}      
    
     
    


void do_mapping(const struct param *pptr, const struct map_param *mptr, const struct fmat *map_s_ptr, int mapset, struct fvec *ras_nl_aspectrum)
{
    char *funcname;
    int i,k;
    int lastfilt;
    float temp;
   
    funcname = "do_mapping";
    lastfilt = pptr->nfilts - pptr->first_good; 
    for (i = pptr->first_good; i<lastfilt; i++)
    {
        temp = 0;
        for (k = 0; k< mptr->n_coefs; k++)
        {
             temp += mptr->reg_coef[mapset][i-pptr->first_good][k] * map_s_ptr->values[i-pptr->first_good][k];
        }
        ras_nl_aspectrum->values[i] = temp;
    } 
}     
