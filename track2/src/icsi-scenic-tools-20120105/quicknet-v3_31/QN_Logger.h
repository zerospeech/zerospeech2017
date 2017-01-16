// $Header: /u/drspeech/repos/quicknet2/QN_Logger.h,v 1.15 2006/03/09 02:09:48 davidj Exp $

#ifndef QN_Logger_h_INCLUDED
#define QN_Logger_h_INCLUDED

#include <QN_config.h>
#include <stdio.h>
#include <stdarg.h>

// This is a base class for handling simple user interactions in command
// line style programs.
// By deriving from this, a user can deal with various events in an
// application-specific way - e.g. send messages to different files.
// The intention is that there is one QN_Logger object per program.

class QN_Logger
{
public:
    // Print an error message and terminate.
    // "objname" is used as a prefix to the message.
    virtual void verror(const char* objname, const char* format,
		       va_list va) = 0;
    // Print a warning message and return.
    // "objname" is used as a prefix to the message.
    virtual void vwarning(const char* objname, const char* format,
			 va_list va) = 0;
    // Output a message to the log file.
    // If "objname" is not NULL, use it as a prefix.
    virtual void vlog(const char* objname, const char* format,
		      va_list va) = 0;
    // Exit successfully.
    virtual void finished() = 0;

    // A more useful interface to the above functions - implemented here
    // to save implementations in all the derived classes.
    void error(const char* objname, const char* format, ...);
    void warning(const char* objname, const char* format, ...);
    void log(const char* objname, const char* format, ...);
    // An alias for log() with objname==NULL.
    void output(const char* format, ...);
};

// The current logging object is called "QN_logger" and is defined elsewhere.

extern QN_Logger* QN_logger;

// Some abbreviations for logger functions.
// Due to the need to support varying numbers of arguments, I cannot think
// of a better way of doing this.

#define QN_OUTPUT QN_logger->output
#define QN_LOG QN_logger->log
#define QN_WARN QN_logger->warning
#define QN_ERROR QN_logger->error
#define QN_FINISH QN_logger->finished

// This is a class that can be used by any classes to control their
// individual logging.

class QN_ClassLogger
{
public:
    QN_ClassLogger(int a_debug, const char* a_objname,
		   const char* a_instname = NULL);
    ~QN_ClassLogger();
    void output(const char* format, ...) const;
    void log(int level, const char* format, ...) const;
    void warning(const char* format, ...) const;
    void error(const char* format, ...) const;
private:
    int debug;			// Level of debugging.
    char* name;			// The name of this logging instance.
    
};

// These are the standard levels of debugging output.

enum {
    QN_LOG_NONE = 0,		// no output
    QN_LOG_PER_RUN = 1,		// once per run
				// e.g. MLP construct
    QN_LOG_PER_EPOCH = 2,	// once per epoch
				// e.g. weight loading and saving
    QN_LOG_PER_SUBEPOCH = 3,	// multiple times per epoch
				// e.g. randomizing weights in a specific layer
				// e.g. reading a load of sentences
    QN_LOG_PER_SENT = 4,	// once per sentence
    QN_LOG_PER_BUNCH = 5,	// once per bunch of frames
    QN_LOG_PER_FRAME = 6,	// once per frame
    QN_LOG_PER_SUBFRAME = 7,	// multiple times per frame
    QN_LOG_PER_VALUE = 8	// log actual data!
};

// A type for streams
enum QN_FileType {
    QN_FILE_UNKNOWN,
    QN_FILE_STDINOUT,
    QN_FILE_FILE,
    QN_FILE_PIPE
};
// A class to remember FILE*->filename and other data mappings
// Very inefficient as it's only used for open/close.
class QN_FileToName
{
public:
    QN_FileToName();
    ~QN_FileToName();
    void create_entry(FILE* afp, const char* afilename,
		      enum QN_FileType atype = QN_FILE_FILE,
		      char* abuf = NULL, const char* atag = NULL);
    char* delete_entry(FILE* afp);
    const char* lookup(FILE* afp); // Map stream to file name
    enum QN_FileType filetype(FILE* afp); // Map stream to stream type.
private:
    enum { MAX_ENTRIES = 16384}; // Most systems have problems with one.
                                      // Process with more than 1K files open.
    FILE *fp[MAX_ENTRIES];	      // An array of file pointers.
    const char* filename[MAX_ENTRIES];   // An array of file names.
    enum QN_FileType type[MAX_ENTRIES]; // Type of the stream.
    char* buf[MAX_ENTRIES];	// Buffer space used by the stream.
    const char* tag[MAX_ENTRIES]; // A tag used to group files.
};

// The one and only instance of the mapper

extern QN_FileToName QN_filetoname;


#define QN_FILE2NAME(fp) QN_filetoname.lookup(fp)

#endif
