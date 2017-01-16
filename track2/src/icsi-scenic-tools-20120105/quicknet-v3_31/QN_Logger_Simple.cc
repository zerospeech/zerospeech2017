#ifndef NO_RCSID
const char* QN_Logger_Simple_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_Logger_Simple.cc,v 1.11 2006/01/28 02:27:52 davidj Exp $";
#endif

// Some routines for standard error handling

/* Must include the config.h file first */
#include <QN_config.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_Logger_Simple.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
#endif

// A simple default logger for non-interactive applications

QN_Logger_Simple::QN_Logger_Simple(FILE* a_log_stream,
				   FILE* a_err_stream,
				   const char* a_progname)
  : progname(a_progname),
    log_stream(a_log_stream),
    err_stream(a_err_stream)
{
#if defined(QN_HAVE_FILENO) && defined(QN_HAVE_ISATTY)
    if (log_stream!=NULL && isatty(fileno(log_stream)))
	log_stream_tty = 1;
    else
	log_stream_tty = 0;
#else
    log_stream_tty = 0;
#endif
}

void
QN_Logger_Simple::verror(const char* objname, const char* format, va_list va)
{
    fputs(progname, err_stream);
    if (objname!=NULL)
	fprintf(err_stream, "(%s)", objname);
    fputs(": ERROR - ", err_stream);
    vfprintf(err_stream, format, va);
    fputc('\n', err_stream);
    // Also send error message to log stream if it is not a tty.
    if (log_stream!=NULL && !log_stream_tty)
    {
	if (objname!=NULL)
	    fprintf(log_stream, "(%s): ", objname);
	fputs("ERROR - ", log_stream);
	vfprintf(log_stream, format, va);
	fputc('\n', log_stream);
    }
    exit(EXIT_FAILURE);
}

void
QN_Logger_Simple::vwarning(const char* objname, const char* format,
			   va_list va)
{
    if (err_stream!=NULL)
    {
	fputs(progname, err_stream);
	if (objname!=NULL)
	    fprintf(err_stream, "(%s)", objname);
	fputs(": WARNING - ", err_stream);
	vfprintf(err_stream, format, va);
	fputc('\n', err_stream);
    }
    if (log_stream!=NULL && !log_stream_tty)
    {
	if (objname!=NULL)
	    fprintf(log_stream, "(%s): ", objname);
	fputs("WARNING - ", log_stream);
	vfprintf(log_stream, format, va);
	fputc('\n', log_stream);
    }
}

void
QN_Logger_Simple::vlog(const char* objname, const char* format, va_list va)
{
    if (log_stream!=NULL)
    {
	if (objname!=NULL)
	{
	    fputs(progname, log_stream);
	    fprintf(log_stream, "(%s)", objname);
	    fputs(": ", log_stream);
	}
	vfprintf(log_stream, format, va);
	fputc('\n', log_stream);
	// Handle problems with the log stream.
	if (ferror(log_stream))
	{
	    fprintf(err_stream, "%s: ERROR - problems writing to log_file"
		    " '%s'.\n", progname, QN_FILE2NAME(log_stream));
	    exit(EXIT_FAILURE);
	}
    }
}

void
QN_Logger_Simple::finished()
{
    exit(EXIT_SUCCESS);
}


