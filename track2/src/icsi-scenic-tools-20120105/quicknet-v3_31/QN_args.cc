#ifndef NO_RCSID
const char *QN_args_rcsid =
  "$Header: /u/drspeech/repos/quicknet2/QN_args.cc,v 1.14 2010/10/29 02:20:37 davidj Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "QN_args.h"

static QN_ArgEntry *get_arg_el(const char *argname, QN_ArgEntry *argtab );
static void show_usage( const char *aname, const QN_ArgEntry *argtab );
static int get_num_args( const QN_ArgEntry *argtab );
static int get_bool_val( const char *value );

static const char* typenames[] = 
{
  "",
  "",
  "int",
  "long",
  "string",
  "boolean",
  "float",
  "double",
  "float,float...",
  "int,int..."
};



int
QN_initargs(QN_ArgEntry *tab, int *argc, const char ***argv,
	char **appname)
{

  const char   *valpos;
  char	       *value, *argname;
  QN_ArgEntry *ael;
  char         *cp;
  const char   *aname;
  int           numargs;	// number of args in argtab
  char         *reqs;
  int           flag;
  int           numset = 0;
  
  aname = (*argv)[0];
  if (appname)
      *appname = (char *) aname; // set the app name 

  numargs = get_num_args(tab);
  reqs = (char *) calloc(numargs, sizeof(char));
  if (reqs==NULL)
  {
      fprintf(stderr,"Error - not enough memory.\n");
      exit(1);
  }
  
  /* skip args past app name, and get the args */
  while ((*argv)++,--*argc)
  {
      
      valpos = strchr(**argv,'=');   /* find the '=' */
      if (!valpos) 
      {
	  /* no more args containing "=" */
	  break;
      }
      /* Make a local copy of arg and val */
      argname = strdup(**argv);
      value = argname + (valpos - **argv);
      /* Add a terminator at the end of the arg part, point value to rest */
      *value++ = '\0';
      if (!(ael = get_arg_el(argname,tab))
	  || (strcmp(argname,"help")==0) 
	  || (strcmp(argname,"HELP")==0))
      {
	  if ((strcmp(argname,"help")!=0) && (strcmp(argname, "HELP")!=0))
	  {
	      fprintf(stderr,"Unknown argument \"%s\"\n",argname);
	  }
	  show_usage(aname,tab);
	  exit(1);
      }
      if (reqs[ael - tab]) 
      {
	  fprintf(stderr,"Error: argument %s multiply set!\n",argname);
	  show_usage(aname,tab);
	  exit(1);
      }
      reqs[ael-tab] = 1;
      ++numset;
      switch (ael->argtype) 
      {
      case QN_ARG_INT:
	  *((int *) ael->varptr) = (int) strtol(value,&cp,10);
	  if (cp == value) 
	  {
	      fprintf(stderr,"Warning: arg %s should be an int!\n",argname);
	      show_usage(aname,tab);
	      exit (1);
	  }
	  break;
      case QN_ARG_LONG:
	  *((long int *) ael->varptr) = strtol(value,&cp,10);
	  if (cp == value) 
	  {
	      fprintf(stderr,"Warning: arg %s should be a long int!\n",argname);
	      show_usage(aname,tab);
	      exit (1);
	  }
	  break;
      case QN_ARG_STR:
	  *((char **) ael->varptr) = (char *) strdup(value);
	  if (reqs==NULL)
	  {
	      fprintf(stderr,"Error - not enough memory.\n");
	      exit(1);
	  }
	  break;
      case QN_ARG_BOOL:
	  *((int *) ael->varptr) = get_bool_val(value);
	  break;
      case QN_ARG_FLOAT:
	  if (!sscanf(value,"%g",((float *) ael->varptr)))
	  {
	      fprintf(stderr,"Warning: arg %s should be a float!\n",argname);
	      show_usage(aname,tab);
	      exit (1);
	  }
	  break;
      case QN_ARG_DOUBLE:
	  if (!sscanf(value,"%lg",((double *) ael->varptr)))
	  {
	      fprintf(stderr,"Warning: arg %s should be a double!\n",argname);
	      show_usage(aname,tab);
	      exit (1);
	  } 
	  break;
      case QN_ARG_LIST_FLOAT:
      {
	  // First work out how much space to allocate.
	  size_t count = 1;
	  size_t i;
	  for (i=0; i<strlen(value); i++)
	  {
	      if (value[i]==',')
		  count++;
	  }
	  // Update the users value.
	  QN_Arg_ListFloat* list =
	      (QN_Arg_ListFloat*) (ael->varptr);
	  float* list_vals;
	  list_vals = new float [count];
	  list->count = count;
	  list->vals = list_vals;
	  // Finally read in the values from the list.
	  char* tok = strtok(value, ",");
	  for (i=0; i<count; i++)
	  {
	      if ((tok==NULL) || !sscanf(tok, "%g", &list_vals[i]))
	      {
		  fprintf(stderr,
			  "Warning: arg %s should be a comma separated "
			  "list of floats.\n",
			  argname);
		  show_usage(aname,tab);
		  exit (1);
	      }
	      tok = strtok(NULL, ",");
	  }
	  break;
      }
      case QN_ARG_LIST_INT:
      {
	  // First work out how much space to allocate.
	  size_t count = 1;
	  size_t i;
	  for (i=0; i<strlen(value); i++)
	  {
	      if (value[i]==',')
		  count++;
	  }
	  // Update the users value.
	  QN_Arg_ListInt* list =
	      (QN_Arg_ListInt*) (ael->varptr);
	  int* list_vals;
	  list_vals = new int[count];
	  list->count = count;
	  list->vals = list_vals;
	  // Finally read in the values from the list.
	  char* tok = strtok(value, ",");
	  for (i=0; i<count; i++)
	  {
	      if ((tok==NULL) || !sscanf(tok, "%i", &list_vals[i]))
	      {
		  fprintf(stderr,
			  "Warning: arg %s should be a comma separated "
			  "list of integers.\n",
			  argname);
		  show_usage(aname,tab);
		  exit (1);
	      }
	      tok = strtok(NULL, ",");
	  }
	  break;
      }
      default:
	  fprintf(stderr,"Internal error!! unknown arg type %d for arg %s!\n",
		  ael->argtype,argname);
	  fprintf(stderr," This should not happen!! contact %s maintainer!\n",
		  aname);
	  exit(1);
	  break;
      }
      /* free the local copy */
      free(argname);
  }
  // now make sure all the required args are set

  flag = 0;
  for (ael = tab, cp = reqs; numargs; ++ael, ++cp, --numargs) 
  {
      if (ael->argreq && !*cp) 
      {
	  fprintf(stderr,"Argument \"%s\" is required, but not set!\n", ael->argname);
	  flag = 1;
      }
  }
  if (flag) 
  {
      show_usage(aname, tab);
      exit(1);
  }
  free(reqs);
  return numset;
  
}


// Given an argument name and an argument table, return the argument table
// entry which ocrresponds to it.

static QN_ArgEntry*
get_arg_el( const char *argname, QN_ArgEntry *argtab )
{
    for (; argtab->argtype != QN_ARG_NOMOREARGS; ++argtab)
    {
	if (argtab->argname && (!strcmp(argtab->argname, argname)) )
	{
	    return argtab;
	}
    }
    return (QN_ArgEntry *) 0;
}


static
void show_usage( const char *aname, const QN_ArgEntry *argtab )
{
    const char* typen;
    fprintf(stderr,"%s:\n",aname);
    for(; argtab->argtype != QN_ARG_NOMOREARGS; ++argtab)
    {
	typen = typenames[argtab->argtype];
	if (argtab->argtype == QN_ARG_DESC) 
	{
	    fprintf(stderr,"  %s\n",argtab->argdesc);
	}
	else
	{
	    fprintf(stderr,"  %s\t%s %s%s%s%s",argtab->argname,argtab->argdesc,
		    *typen?"(":"", typen, *typen?")":"",
		    argtab->argreq?" (*REQ*)":"");
	    // print default value.
	    //Note only non-required options have default values
	    if (!argtab->argreq) 
	    {
		switch (argtab->argtype) 
		{
		case QN_ARG_INT:
		    fprintf(stderr," (%d)", *((int *)argtab->varptr));
		    break;
		case QN_ARG_LONG:
		    fprintf(stderr," (%ld)", *((long int *) argtab->varptr));
		    break;
		case QN_ARG_STR:
		    fprintf(stderr," (%s)",
			    *((char **) argtab->varptr) ?
			    *((char **)argtab->varptr) : "null");
		    break;
		case QN_ARG_BOOL:
		    fprintf(stderr," (%s)", *((int *) argtab->varptr) ?
			    "true" : "false");
		    break;
		case QN_ARG_FLOAT:
		    fprintf(stderr," (%g)", *((float *) argtab->varptr));
		    break;
		case QN_ARG_DOUBLE:
		    fprintf(stderr," (%g)", *((double *) argtab->varptr));
		    break;
		case QN_ARG_LIST_FLOAT:
		{
		    int i;
		    QN_Arg_ListFloat* list =
			(QN_Arg_ListFloat*) (argtab->varptr);

		    fprintf(stderr, " (");
		    for (i=0; i<list->count; i++)
		    {
			if (i!=0)
			    fputc(',', stderr);
			fprintf(stderr, "%g", (list->vals)[i]);
		    }
		    fprintf(stderr, ")");
		    break;
		}
		case QN_ARG_LIST_INT:
		{
		    int i;
		    QN_Arg_ListInt* list =
			(QN_Arg_ListInt*) (argtab->varptr);

		    fprintf(stderr, " (");
		    for (i=0; i<list->count; i++)
		    {
			if (i!=0)
			    fputc(',', stderr);
			fprintf(stderr, "%d", (list->vals)[i]);
		    }
		    fprintf(stderr, ")");
		    break;
		}
		default:
		    fprintf(stderr,
			    "Internal error! Unknown arg type %d for arg %s\n",
			    argtab->argtype,argtab->argname);
		    fprintf(stderr," This should not happen! "
			    "contact %s maintainer!\n", aname);
		    abort();
		    break;
		}
		
		
	    }
	    fprintf(stderr,"\n");
	}
    }
}

// Return TRUE if the string pointed to by value is a "true" string, false
// otherwise.  In this implementation, if the string is "1", or begins with a
// "t" or a "y" (any case), it is true, otherwise it is false
// the NULL or empty string are each false, obviously

static int
get_bool_val( const char *value )
{
  char c;
  if (value) 
  {
    c = *value;
    return (c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y');
  }
  return(0);
}


// count the number of elements in the argtab

static int
get_num_args( const QN_ArgEntry *argtab )
{
    int i;

    for(i = 0; argtab->argtype != QN_ARG_NOMOREARGS; ++argtab)
    {
	++i;
    }
    return (i);
}


// This function will print all the arguments and values as set to a given
// file (STDERR if fp is NULL).  This function will be useful for diagnostic
// output, e.g. to log files.   If appname is not null, it will display that,
// too.

void
QN_printargs(FILE *fp, const char *appname, const QN_ArgEntry *argtab)
{
    int i = 0;
    if (fp == 0) 
    {
	fp = stderr;
    }
    fprintf(fp,"%s was invoked with the following argument values\n (some of"
	    " which may be defaults):\n",appname?appname:"This");
    for(i = 0; argtab->argtype != QN_ARG_NOMOREARGS; ++argtab)
    {
	if (argtab->argtype == QN_ARG_DESC)
	    continue;
	fprintf(fp,"  %s=",argtab->argname);
	switch (argtab->argtype) 
	{
	case QN_ARG_INT:
	    fprintf(fp,"%d", *((int *)argtab->varptr));
	    break;
	case QN_ARG_LONG:
	    fprintf(fp,"%ld", *((long int *) argtab->varptr));
	    break;
	case QN_ARG_STR:
	    fprintf(fp,"%s", *((char **) argtab->varptr)?*((char **)
							   argtab->varptr):
		    "(null)");
	    break;
	case QN_ARG_BOOL:
	    fprintf(fp,"%s", *((int *) argtab->varptr)?"true":"false");
	    break;
	case QN_ARG_FLOAT:
	    fprintf(fp,"%g", *((float *) argtab->varptr));
	    break;
	case QN_ARG_DOUBLE:
	    fprintf(fp,"%g", *((double *) argtab->varptr));
	    break;
	case QN_ARG_LIST_FLOAT:
	{
	    int i;
	    QN_Arg_ListFloat* list =
		(QN_Arg_ListFloat*) (argtab->varptr);
	    
	    for (i=0; i<list->count; i++)
	    {
		if (i!=0)
		    fputc(',', fp);
		fprintf(fp, "%g", (list->vals)[i]);
	    }
	    break;
	}
	case QN_ARG_LIST_INT:
	{
	    int i;
	    QN_Arg_ListInt* list =
		(QN_Arg_ListInt*) (argtab->varptr);
	    
	    for (i=0; i<list->count; i++)
	    {
		if (i!=0)
		    fputc(',', fp);
		fprintf(fp, "%d", (list->vals)[i]);
	    }
	    break;
	}
	default:
	    fprintf(fp,"Internal error!! unknown arg type %d for arg %s!\n",
		    argtab->argtype,argtab->argname);
	    fprintf(fp," This should not happen!! contact %s maintainer!\n",
		    appname?appname:"its");
	    exit(1);
	    break;
	}
	/* Put a backslash here so cut and paste can be used to reuse args */
	fprintf(fp," \\\n");
    }
	fprintf(fp,"\n");
}


