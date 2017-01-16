/*
** $Header: /u/drspeech/repos/quicknet2/QN_config_tail.h,v 1.2 2004/10/06 21:10:26 davidj Exp $
** 
** QN_config_tail.h - stuff to add onto the end of QN_config.h
*/

#if defined(QN__FILE_OFFSET_BITS) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS QN__FILE_OFFSET_BITS
#endif
#if defined(QN__LARGEFILE_SOURCE) && !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE QN__LARGEFILE_SOURCE
#endif
#if defined(QN__LARGE_FILES) && !defined(_LARGE_FILES)
#define _LARGE_FILES QN__LARGE_FILES
#endif
