const char* QN_misc_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_misc.cc,v 1.8 2010/10/29 02:20:38 davidj Exp $";

/* Must include the config.h file first */
#include <QN_config.h>
#include <stdio.h>
#include <stddef.h>

#include "QN_types.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
#endif

const char* QN_progname = "QuickNet";
FILE* QN_logfile = stdout;

