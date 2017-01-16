#ifndef NO_RCSID
const char* QN_Logger_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_Logger.cc,v 1.8 2006/03/09 02:09:47 davidj Exp $";
#endif

// User interaction routines.

/* Must include the config.h file first */
#include <QN_config.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "QN_types.h"
#include "QN_Logger.h"
#include "QN_utils.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#define EXIT_FAILURE (1)
#endif

////////////////////////////////////////////////////////////////
// Friendly interfaces to the va_list based logging functions.

void
QN_Logger::error(const char* objname, const char* format, ...)
{
    va_list va;

    va_start(va, format);
    verror(objname, format, va);
    va_end(va);
}

void
QN_Logger::warning(const char* objname, const char* format, ...)
{
    va_list va;

    va_start(va, format);
    vwarning(objname, format, va);
    va_end(va);
}

void
QN_Logger::log(const char* objname, const char* format, ...)
{
    va_list va;

    va_start(va, format);
    vlog(objname, format, va);
    va_end(va);
}

void
QN_Logger::output(const char* format, ...)
{
    va_list va;

    va_start(va, format);
    vlog(NULL, format, va);
    va_end(va);
}

////////////////////////////////////////////////////////////////

QN_ClassLogger::QN_ClassLogger(int a_debug, const char* a_objname,
			       const char* a_instname)
    : debug(a_debug)
{
    size_t len = 0;
    len += ((a_objname!=NULL) ? strlen(a_objname)+1 : 0);
    len += ((a_instname!=NULL) ? strlen(a_instname)+1 : 0);
    name = new char[len];
    name[0] = '\0';
    if (a_objname!=NULL)
	strcat(name, a_objname);
    if (a_instname!=NULL)
    {
	strcat(name, ".");
	strcat(name, a_instname);
    }
}

QN_ClassLogger::~QN_ClassLogger()
{
    delete[] name;
}

void
QN_ClassLogger::output(const char* format, ...) const
{
    va_list va;
    
    va_start(va, format);
    QN_logger->vlog(NULL, format, va);
    va_end(va);
}

void
QN_ClassLogger::log(int level, const char* format, ...) const
{
    va_list va;
    
    va_start(va, format);
    if (debug>=level)
    {
	QN_logger->vlog(name, format, va);
    }
    va_end(va);
}

void
QN_ClassLogger::error(const char* format, ...) const
{
    va_list va;
    
    va_start(va, format);
    QN_logger->verror(name, format, va);
    va_end(va);
}

void
QN_ClassLogger::warning(const char* format, ...) const
{
    va_list va;
    
    va_start(va, format);
    QN_logger->vwarning(name, format, va);
    va_end(va);
}


////////////////////////////////////////////////////////////////

QN_FileToName::QN_FileToName()
{
    size_t i;

    for (i=0; i<MAX_ENTRIES; i++)
    {
	fp[i] = NULL;
    }
}

QN_FileToName::~QN_FileToName()
{
    size_t i;

    for (i=0; i<MAX_ENTRIES; i++)
    {
	if (fp[i] !=NULL)
	{
	    assert(filename[i]!=NULL);
// Cannot use this warning as most error exits don't close files.	    
//	    QN_WARN("QN_FileToName", "file %s not closed before exit.",
//		    filename[i]);
	    delete[] filename[i];
	}
    }
}

void
QN_FileToName::create_entry(FILE* afp, const char* afilename,
			    enum QN_FileType atype,
			    char* abuf, const char* atag)
{
    size_t i;

    if (afp==stdin || afp==stdout || afp==stderr)
	return;
    for (i=0; i<MAX_ENTRIES; i++)
    {
	if (fp[i]==NULL)
	{
	    fp[i] = afp;
	    filename[i] = QN_strdup(afilename);
	    type[i] = atype;
	    buf[i] = abuf;
	    if (atag!=NULL)
		tag[i] = QN_strdup(atag);
	    else
		tag[i] = atag;
	    return;
	}
    }
    QN_ERROR("QN_FileToName", "failed to find space for new filename in create_entry.");
}

char*
QN_FileToName::delete_entry(FILE* afp)
{
    size_t i;
    char* ret;

    if (afp==stdin || afp==stdout || afp==stderr)
	return NULL;
    for (i=0; i<MAX_ENTRIES; i++)
    {
	if (fp[i]==afp)
	{
	    fp[i] = NULL;
	    delete [] filename[i];
	    filename[i] = NULL;
	    ret = buf[i];
	    buf[i] = NULL;
	    if (tag[i]!=NULL)
	    {
		delete[] tag[i];
		tag[i] = NULL;
	    }
	    return ret;
	}
    }
    QN_ERROR("QN_FileToName", "failed to find file in delete_entry.");
    return NULL;
}

const char*
QN_FileToName::lookup(FILE* afp)
{
    size_t i;

    if (afp==stdin)
	return("(stdin)");
    else if (afp==stdout)
	return("(stdout)");
    else if (afp==stderr)
	return("(stderr)");
    else
    {
	for (i=0; i<MAX_ENTRIES; i++)
	{
	    if (fp[i]==afp)
	    {
		return(filename[i]);
	    }
	}
    }
    // Only here if we cannot find an entry.
    // Note that this is not necessarily an error as programs using the
    // library may have used "fopen" rather than "QN_open".
    return("(unknown)");
}

enum QN_FileType
QN_FileToName::filetype(FILE* afp)
{
    size_t i;

    if (afp==stdin)
	return(QN_FILE_STDINOUT);
    else if (afp==stdout)
	return(QN_FILE_STDINOUT);
    else if (afp==stderr)
	return(QN_FILE_STDINOUT);
    else
    {
	for (i=0; i<MAX_ENTRIES; i++)
	{
	    if (fp[i]==afp)
	    {
		return(type[i]);
	    }
	}
    }
    // Only here if we cannot find an entry.
    // 
    // Not necessarily an error - something may have used
    // "fopen" rather than "QN_open".
    return(QN_FILE_UNKNOWN);
}

// The actuall FileToName object.

QN_FileToName QN_filetoname;
