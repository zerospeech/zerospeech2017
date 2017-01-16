#ifndef NO_RCSID
const char* QN_defaultlogger_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_defaultlogger.cc,v 1.2 1995/07/25 18:59:45 davidj Exp $";
#endif

#include "QN_Logger.h"
#include "QN_Logger_Simple.h"

// A default logger - will be linked in if the user does not define one

static QN_Logger_Simple QN_defaultlogger(stdout, stderr, "QuickNet");
QN_Logger* QN_logger = &QN_defaultlogger;

