//
// ExecUtils.C
//
// A few routines to provide some of the functionality of Tcl's "exec" 
// for a C++ program (i.e. convert a string into the stdout of it 
// when executed).
//
// 1997aug14 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/ExecUtils.C,v 1.5 2012/01/05 20:30:15 dpwe Exp $

// Originally based on /u/drspeech/src/berpdemo/berpbackend/dbquery.C

#include "ExecUtils.H"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
// for kill (called on child in desperation)
#include <signal.h>
// for fcntl (to help childs let go, like Tcl)
#include <fcntl.h>
// for waitpid()
#include <sys/wait.h>

#ifdef DEBUG
#define DBGFPRINTF(a) fprintf a
#else /* !DEBUG */
#define DBGFPRINTF(a) /* nothing */
#endif /* DEBUG */

/* The assignment of the two-element fd array for pipe */
#define PIPE_READ 0
#define PIPE_WRITE 1

int exuCreateCommandPipes(char *cmd, char *argv[], FILE **myin, FILE **myout)
{   /* Execute the named command with the specified arguments, 
       and return files to its input and output.
       Return child pid on success, 0 if the command couldn't launch. */
    int code;
    int child;
    int cmd_fds[2];
    int rslt_fds[2];

    code=pipe(cmd_fds);
    if (code == -1) {
	perror("dbquery: pipe");
	exit(1);
    }
    DBGFPRINTF((stderr,"opened pipe 1\n"));
    code=pipe(rslt_fds);
    if (code == -1) {
	perror("dbquery: pipe");
	exit(1);
    }
    DBGFPRINTF((stderr,"opened pipe 2\n"));

    if ((child=vfork()) > 0 ) {
	DBGFPRINTF((stderr,"inside parent\n"));
	close(cmd_fds[PIPE_READ]);
	close(rslt_fds[PIPE_WRITE]);
	DBGFPRINTF((stderr,"closed descriptors\n"));

	*myout = fdopen(rslt_fds[PIPE_READ],"r");
	*myin = fdopen(cmd_fds[PIPE_WRITE],"w");
	DBGFPRINTF((stderr,"fdopened read/write\n"));

	return child;
    } else if (child==0) {
	DBGFPRINTF((stderr,"inside child\n"));
	close(cmd_fds[PIPE_WRITE]);    
	close(rslt_fds[PIPE_READ]);
	DBGFPRINTF((stderr,"closed descriptors\n"));

	if (dup2(cmd_fds[PIPE_READ], STDIN_FILENO) == -1) {
	    perror("dbquery: child: dup2(stdin)");
	}
	DBGFPRINTF((stderr,"renamed stdin\n"));
	if (dup2(rslt_fds[PIPE_WRITE], STDOUT_FILENO) == -1) {
	    perror("dbquery: child: dup2(stdout)");
	}
	DBGFPRINTF((stderr,"renamed stdout\n"));
	/* some stuff from tcl7.6/unix/tclUnixPipe.c.. */
	/* maybe send stderr to stdout? */
	/* dup2(1,2); */
	/* or just close stderr ?? */
	/* close(STDERR_FILENO); */
	/* don't do both! */

	/* Must clear the close-on-exec flag for the target FD, since
	 * some systems do not clear the CLOEXEC flag on the target FD. */
	fcntl(STDIN_FILENO, F_SETFD, 0);
	fcntl(STDOUT_FILENO, F_SETFD, 0);
	fcntl(STDERR_FILENO, F_SETFD, 0);

	DBGFPRINTF((stderr,"CLOEXEC, execing process\n"));

	DBGFPRINTF((stderr, "execv(%s %s ...)\n", cmd, argv[1]?argv[1]:""));
	execv(cmd, argv);
	DBGFPRINTF((stderr, "execv fell through\n"));
	close(cmd_fds[PIPE_READ]);
	close(rslt_fds[PIPE_WRITE]);
	_exit(0);	/* not exit() - see "man vfork" */
    } else {
	perror("dbquery: vfork failed");
	exit(1);
    }
}

#define MAXPATHLEN 1024

int exuSearchExecPaths(char *cmd, char *paths, char *rslt, int rsltlen)
{   /* Search through the WS or colon-separated list of paths
       for an executable file whose tail is cmd.  Return absolute 
       path in rslt, which is allocated at rsltlen.  Return 1 
       if executable found & path successfully returned, else 0. */
    char testpath[MAXPATHLEN];
    int cmdlen = strlen(cmd);
    
    /* zero-len testpath means not found, so preset it that way */
    testpath[0] = '\0';
    /* If the cmd contains a slash, don't search paths */
    if (strchr(cmd, '/') != NULL) {
	/* if it *starts* with a "/", treat it as absolute, else prepend cwd */
	if(cmd[0] == '/') {
	    assert(cmdlen+1 < MAXPATHLEN);
	    strcpy(testpath, cmd);
	} else {
	    testpath[0] = '\0';
	    getcwd(testpath, MAXPATHLEN);
	    assert(strlen(testpath) > 0 \
		   && strlen(testpath)+cmdlen+2 < MAXPATHLEN);
	    strcat(testpath, "/");
	    strcat(testpath, cmd);
	}    
	/* See if this path is executable */
	//fprintf(stderr, "testing '%s'...\n", testpath);
	if(access(testpath, X_OK) != 0) {
	    /* exec access failed, fall through with zero-len */
	    testpath[0] = '\0';
	}
    } else {  /* no slash in cmd, search the path components */
	/* Loop through the pieces of <paths> trying each one */
	int plen;
	while(strlen(paths)) {
	    /* Skip over current separator (or excess WS) */
	    paths += strspn(paths, ": \t\r\n");
	    /* extract next path */
	    plen = strcspn(paths, ": \t\r\n");
	    if(plen) {
		assert(plen+cmdlen+2 < MAXPATHLEN);
		/* copy the path part */
		strncpy(testpath, paths, plen);
		/* shift on so next path is grabbed next time */
		paths += plen;
		/* build the rest of the path */
		testpath[plen] = '/';
		strcpy(testpath+plen+1, cmd);
		//fprintf(stderr, "testing '%s'...\n", testpath);
		/* see if it's there */
                if(access(testpath, X_OK) != 0) {
	            /* exec access failed, fall through with zero-len */
		    testpath[0] = '\0';
		} else {
		    /* found the ex, drop out' the loop */
		    break;
		}
	    }
	}
    }
    /* Now we either have a working path in testpath, or we found nothing */
    if(strlen(testpath) == 0) {
	return 0;
    } else {
	if(rslt) {	/* we have a string to copy to */
	    if(strlen(testpath)+1 < rsltlen) {
		strcpy(rslt, testpath);
	    } else {
		fprintf(stderr, "SearchExec: rsltlen (%d) too small for %ld\n", rsltlen, strlen(testpath));
		return 0;
	    }
	}
	return 1;
    }
}

#define MAXTOKEN 256

int exuParseToArgv(char *cmd, char *argv[], int maxargv)
{   /* Take a single string with spaces and parse it into an argv array;
       Basically, break the string up on white space, but allow quoting.
       Fill in upt to maxargv elements in the pre-allocated argv array, 
       but return the actual argc we would have if argv were large enough.  
       Return 0 under error conditions. */
    int argc = 0;
    int curlen, seglen;
    char currentToken[MAXTOKEN];
    char qchr = '\0';		/* the quotes we're currently in */

    currentToken[0] = '\0';
    /* skip over initial separator */
    cmd += strspn(cmd, " \t\r\n");
    while(strlen(cmd)) {
	/* build up a single token */
	curlen = strlen(currentToken);
	/* find next interesting chr */
	seglen = strcspn(cmd, " \t\r\n\\`\'\"");
//fprintf(stderr, "P2A: len=%d ct='%s' cmd='%s'\n", seglen, currentToken, cmd);
	/* handle backslash-quoting */
	if(cmd[seglen] == '\\') {
	    /* copy what we have so far */
	    strncpy(currentToken+curlen, cmd, seglen);
	    cmd += seglen;
	    /* then take the quoted character */
	    if(cmd[1]) {	/* (cmd[0] is the "\") */
		currentToken[curlen+seglen] = cmd[1];
		++cmd;
	    } else {  /* following chr was EOString, take the slash */
		currentToken[curlen+seglen] = cmd[0];
	    }
	    /* skip the char copied */
	    ++cmd;
	    /* end the string */
	    currentToken[curlen+seglen+1] = '\0';
	    /* Keep looping after de-escaping */
	} else if (cmd[seglen] == '\"' || cmd[seglen] == '\'' \
		   || cmd[seglen] == '`') {
	    if (qchr == cmd[seglen]) {
		/* end of current quoting - take the last part */
		strncpy(currentToken+curlen, cmd, seglen);
		currentToken[curlen+seglen] = '\0';
		/* skip past the quote */
		cmd += seglen+1;
		/* check it's truly end-of-token */
		if(cmd[0] != '\0' && strspn(cmd, " \t\r\n") == 0) {
		    fprintf(stderr, "ParseToArgv: extra characters after close quote\n");
		    return 0;
		}
		/* mark out of quoting */
		qchr = '\0';
	    } else if (qchr == '\0' \
		       // always ignore QUOTE'S as a quote now ...
		       && curlen+seglen == 0) {
		/* start quoting */
		if(curlen+seglen > 0) {
		    // fprintf(stderr, "ParseToArgv: quote in middle of token\n");
		    // return 0;
		    // No, just treat embedded quotes as regular chrs
		} else {
		    /* else we're fine - remember which type of quote & skip on */
		    qchr = cmd[seglen];
		    ++cmd;
		}
	    } else {
		/* enclosing one kind of quote within another - OK I guess */
		++seglen;	/* include the quote */
		strncpy(currentToken+curlen, cmd, seglen);
		currentToken[curlen+seglen] = '\0';
		cmd += seglen;
	    }
	} else {
	    /* span was up to a white-space */
	    /* copy what we spanned */
	    strncpy(currentToken+curlen, cmd, seglen);
	    currentToken[curlen+seglen] = '\0';
	    cmd += seglen;
	    if (qchr) {	       	/* we're inside quotes; don't end token */
		if(cmd[0] == '\0') {	/* end of string? */
		    fprintf(stderr, "ParseToArgv: unterminated %c\n", qchr);
		    return 0;
		}
		/* else copy the within-quotes whitespace */
		currentToken[curlen+seglen] = cmd[0];
		currentToken[curlen+seglen+1] = '\0';
		++cmd;
	    } else {
		/* end the token */
		if(argc < maxargv) {
		    argv[argc] = strdup(currentToken);
		}
		++argc;		/* keep parsing & counting even after 
				   we run out of slots in argv */
		currentToken[0] = '\0';
		/* skip over white-space before next token */
		cmd += strspn(cmd, " \t\r\n");
	    }
	}
    }
    /* mark the end of the set */
    if(argc < maxargv) {
	argv[argc] = NULL;
    }
    return argc;
}

char* exuExecCmd(char *cmd)
{   /* Take a command and run it as passed; return its stdout 
       as a single, newly-allocated string, with any trailing 
       newline stripped (but interior newlines kept).  Return NULL 
       on error. */
    char *argv[256];
    int  argc;
    char *path;
    FILE *outf, *inf;
    char buf[2048];
    int  allocStep = 256;
    char *rtn;
    int opAlloc = 0, got = 0, newgot;

    argc = exuParseToArgv(cmd, argv, 256);
    if(argc < 1) {
	/* nothing in the command string */
	return 0;
    }

#ifdef DEBUG
    fprintf(stderr, "ExecCmd: DEBUG: argv for execv is:\n");
    int i = 0;
    while(argv[i] != NULL) {
	fprintf(stderr, "arg %d = '%s'\n", i, argv[i]);
	++i;
    }
#endif /* DEBUG */

    char cmdpath[256];
    if(!exuSearchExecPaths(argv[0], getenv("PATH"), cmdpath, 256)) {
	fprintf(stderr, "ExecCmd: couldn't find executable '%s'\n",
		argv[0]);
	return 0;
    }
    int child;
    if( (child = exuCreateCommandPipes(cmdpath, argv, &outf, &inf)) == 0) {
	/* error opening pipes?? */
	fprintf(stderr, "ExecCmd: error launching '%s'\n",
		cmd);
	return 0;
    }
    /* close the command ouptut pipe immediately since we won't be
       sending anything (right?) */
    fclose(outf);
    /* Allocate buffer for output */
    opAlloc = allocStep;
    rtn = (char *)malloc(opAlloc);
    while (!feof(inf)) {
	if(fgets(buf, 2048, inf) != NULL) {
	    newgot = strlen(buf);
	    while(newgot+got+1 > opAlloc) {
		opAlloc += allocStep;
		rtn = (char *)realloc(rtn, opAlloc);
	    }
	    memcpy(rtn+got, buf, newgot);
	    got += newgot;
	}
    }
    /* close the result reading pipe */
    fclose(inf);

    /* get rid of clinging child */
    /* kill(child, SIGKILL); */
    /* no, wait for child to die of natural causes */
    int waitrc, stat;
    int opts = 0;	/* or WNOHANG? */
    /* This does the trick.  Wait for the child to change state i.e. die */
    waitrc = waitpid(child, &stat, opts);
    DBGFPRINTF((stderr, "wait(%d,x,%d)=%d\n", child, opts, waitrc));
    
    /* remove the final newline */
    if(rtn[got-1] == '\n') {
	--got;
    }
    /* terminate the string */
    rtn[got] = '\0';
    return rtn;
}

/* from aprl/tclScheduler.c .. */

int exuExpandPercents(const char *input, char *output, int outlen, 
		      int nsubs, const char *subchrs, char **substrs)
{	/* Take a string containing %x type strings, and expand them.  
	   Recognize <nsubs> substitutions, defined by subchrs[i]; 
	   %<subchrs[i]> becomes <substrs[i]>. 
	   <output> is pre-allocated output string, <outlen> long; 
	   return 0 if that's not big enough. */
    int outrem = outlen;
    int ncpy;

#define _ADDOUT(N,FROM)	if(N>=outrem)	return 0;	   \
                        else {  strncpy(output, FROM, N);  \
				output += N; *output = 0; outrem -= N; }

    *output = 0;
    while(1)  {
        const char *pct = strchr(input, '%');
	if(pct==NULL)  {
	    ncpy = strlen(input);
	    _ADDOUT(ncpy, input);
	    *output++ = 0;
	    return 1;
	} else {	// found a percent; copy up to it
	    ncpy = pct-input;
	    _ADDOUT(ncpy, input);
	    input += ncpy; 
	    // find the substitution
	    char subc = input[1];
	    int i=0, didsub = 0;
	    while(i<nsubs && !didsub)  {
		if(subchrs[i] == subc)  {
		    ncpy = strlen(substrs[i]);
		    _ADDOUT(ncpy,substrs[i]);
		    didsub = 1;
		}
		++i;
	    }
	    if(!didsub)  {
		_ADDOUT(2, input);
	    }
	    // skip over the "%x" in input, however we dealt with it.
	    input += 2;
	}
    }
#undef ADDOUT
}

#ifdef MAIN
/* test code */

int main(int argc, char *argv[]) {
    /* test the ExecCmd function */
    int i, count = 1;

    if(argc < 2) {
	fprintf(stderr, "usage: %s cmdstring [count]\n", argv[0]);
	exit(1);
    }
    if (argc > 2) {
	count = atoi(argv[2]);
    }

    char *rslt;

    for (i = 0; i < count; ++i) {
	rslt = exuExecCmd(argv[1]);
	if(rslt) {
	    fprintf(stderr, "got: '%s'\n", rslt);
	    free(rslt);
	} else {
	    fprintf(stderr, "no result\n");
	}
    }
}

#define BUFFER_SIZE 5000

int oldmain(int argc, char *argv[]) {
    /* test the exec call */
    FILE *inf, *outf;
    char buf[BUFFER_SIZE];

    if(argc < 3) {
	fprintf(stderr, "usage: %s input cmd argstring\n", argv[0]);
	exit(1);
    }
    assert(argv[argc] == NULL);

    char cmd[MAXPATHLEN];
    if(!exuSearchExecPaths(argv[2], getenv("PATH"), cmd, MAXPATHLEN)) {
	fprintf(stderr, "%s: couldn't find executable '%s'\n", 
		argv[0], argv[2]);
	exit(-1);
    }
//    my_open(cmd, argv+2);	/* passed argv[0] is same as cmd */
    
#define MAXARGS 5
    int i, myargc;
    char *myargv[MAXARGS+1];
    myargc = exuParseToArgv(argv[3], myargv+1, MAXARGS);
    for(i = 0; i < myargc; ++i) {
	fprintf(stderr, "arg %d = '%s'\n", i, myargv[1+i]);
    }
    /* add the command name (unexpanded) as the first arg */
    myargv[0] = argv[2];
    if(exuCreateCommandPipes(cmd, myargv, &outf, &inf)) {
	DBGFPRINTF((stderr, "abt to write: '%s'\n", argv[1]));
	if(argv[1][0]) {
	    fputs(argv[1], outf);
	    fputs("\n", outf);
	    fflush(outf);
	}
	DBGFPRINTF((stderr, "abt to wait for fgets\n"));
	fgets(buf, BUFFER_SIZE, inf);

	fprintf(stderr, "got: '%s'\n", buf);
    }

    exit(0);

    return 1;
}

#endif /* MAIN */
