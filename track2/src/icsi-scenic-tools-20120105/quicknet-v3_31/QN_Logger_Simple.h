// $Header: /u/drspeech/repos/quicknet2/QN_Logger_Simple.h,v 1.7 1996/10/30 00:28:26 davidj Exp $

#ifndef QN_Logger_Simple_h_INCLUDED
#define QN_Logger_Simple_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stdarg.h>
#include "QN_Logger.h"

// A simple default logger for non-interactive applications

class QN_Logger_Simple : public QN_Logger
{
public:
    QN_Logger_Simple(FILE* a_log_stream, FILE* a_err_stream,
		     const char* a_progname);
    virtual ~QN_Logger_Simple() {};
    virtual void verror(const char* objname, const char* format, va_list va);
    virtual void vwarning(const char* objname,const char* format,va_list va);
    virtual void vlog(const char* objname, const char* format, va_list va);
    virtual void finished();
private:
    const char* progname;
    FILE* log_stream;
    FILE* err_stream;
    int log_stream_tty;		// Non-zero if log stream is a tty.
};

#endif
