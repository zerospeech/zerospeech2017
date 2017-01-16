// $Header: /u/drspeech/repos/quicknet2/QN_args.h,v 1.5 2006/02/08 03:18:41 davidj Exp $


#ifndef QN_args_h_INCLUDED
#define QN_args_h_INCLUDED


// Author:  Jonathan Segal  jsegal@icsi.berkeley.edu
// Modified for QuickNet by: davidj@icsi.berkeley.edu
// This a quick, hacked argument handling package taken from icsilib
//
// This will be replaced by something more substantial in the future
// (davidj - Wed Oct 18 22:45:13 1995)


// QN_ArgType denotes the type of the variable which corresponds to the
// argument.

enum QN_ArgType
{
  QN_ARG_NOMOREARGS,
  QN_ARG_DESC,
  QN_ARG_INT,
  QN_ARG_LONG,
  QN_ARG_STR,
  QN_ARG_BOOL,
  QN_ARG_FLOAT,
  QN_ARG_DOUBLE,
  QN_ARG_LIST_FLOAT,
  QN_ARG_LIST_INT,
  QN_ARG_NUMARGTYPES
};

enum QN_ArgReq
{
   QN_ARG_OPT,			// argument is optional
   QN_ARG_REQ			// argument is required
};

// A structure for holding lists of floats - a hack for learning rates.

struct QN_Arg_ListFloat
{
    int count;
    float* vals;
};

//
struct QN_Arg_ListInt
{
    int count;
    int* vals;
};


struct QN_ArgEntry
{
  const char * argname;		// name of the argument
  const char * argdesc;		// text description of the arg (for usage info)
  QN_ArgType    argtype;	// type of the argument
  void        *varptr;          // the variable which gets the arg's value 
  QN_ArgReq   argreq;           // is the argument required?
};


/*
  QN_initargs is the main function you call to actually parse the arguments as
  given.  To use this function, you must first set up an argument
  descriptor table tab.  This table is a NULL-terminated array of elements of
  QN_ArgEntry.  Each QN_ArgEntry is a structure containing the following
  elements:
  const char *argname; the name of the argument, a string
  const char *argdesc; a textual description, of the arg, displayed as help
  QN_ArgType argtype;  the type of the argument, see below
  void *varptr;        a pointer to the variable which will contain the
                       specified value
  QN_ArgReq argreq;    specifies if the argument is required or optional
                       (the default is for an argument to be optional)

  argtype can be one of QN_ARG_INT, QN_ARG_LONG, QN_ARG_STR, QN_ARG_BOOL,
  QN_ARG_FLOAT, QN_ARG_DOUBLE, QN_ARG_LIST_FLOAT, QN_ARG_LIST_INT,
  or QN_ARG_DESC.  QN_ARG_DESC entries are dummy
  entries which you can use to supply more documentation in the usage message.
  The current paradigm for QN_ARG_BOOL is that the variable must be a C int,
  and if the value of a variable is specified as "1", "[tT].*" or "[yY].*",
  the variable will be set to TRUE, otherwise it will be set to false.  for
  QN_ARG_STR, the variable will be set to a malloced copy of the string
  specified on the command-line.  argreq is one of QN_ARG_REQ or QN_ARG_OPT
  (note: if you leave this out of a static initializer for the structure,
  QN_ARG_OPT will be the default).  If it is QN_ARG_REQ the argument will be
  required to be specified, if it is QN_ARG_OPT, the argument is optional.
  QN_initargs will parse the command-line, and will set the variables named in
  the argument table to the values specified on the command-line.  If appname
  is non-null, it will set the variable pointed to by appname to be a pointer
  to a malloced copy of the name which the application was called by.  It will
  modify argc and argv, eliminating the references to the arguments with "="
  in them.  The remaining argc positional arguments will be left in the
  variable argv.  Note: if somebody types a command of the form % command
  arg1=5 foo arg2=7 icsiargs will set the variable referred to by arg1 to 5,
  and will set argc to 2 and argv to the array {"foo","arg2=7",NULL}

  QN_initargs will return the number of arguments it successfully set.
*/

int
QN_initargs(QN_ArgEntry *tab, int *argc, const char ***argv, char **appname );



/*
  This function will print all the arguments and values as set to a given
  file (STDERR if fp is NULL).  This function will be useful for diagnostic
  output, e.g. to log files.   If appname is not null, it will display that,
  too.
*/

void
QN_printargs(FILE *fp, const char *appname, const QN_ArgEntry *argtab);

#endif
