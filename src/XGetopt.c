// XGetopt.cpp  Version 1.1
//
// Author:  Hans Dietrich
//          hdietrich2@hotmail.com
//
// Modified: David Smith
//           dave.s@earthcorp.com
//           Moved two char declarations from body of function so
//           that it can compile as a C function.
//           Thanks so much Hans
//
// This software is released into the public domain.
// You are free to use it in any way you like.
//
// This software is provided "as is" with no expressed
// or implied warranty.  I accept no liability for any
// damage or loss of business that this software may cause.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef __MINGW32__		/* mingw has getopt() */

#include "define.h"


///////////////////////////////////////////////////////////////////////////////
//
//  X G e t o p t . c p p
//
//
//  NAME
//       getopt -- parse command line options
//
//  SYNOPSIS
//       int getopt(int argc, char* const* argv, const char *optstring)
//
//       extern char *optarg;
//       extern int optind;
//
//  DESCRIPTION
//       The getopt() function parses the command line arguments. Its
//       arguments argc and argv are the argument count and array as
//       passed into the application on program invocation.  In the case
//       of Visual C++ programs, argc and argv are available via the
//       variables __argc and __argv (double underscores), respectively.
//       getopt returns the next option letter in argv that matches a
//       letter in optstring.
//
//       optstring is a string of recognized option letters;  if a letter
//       is followed by a colon, the option is expected to have an argument
//       that may or may not be separated from it by white space.  optarg
//       is set to point to the start of the option argument on return from
//       getopt.
//
//       Option letters may be combined, e.g., "-ab" is equivalent to
//       "-a -b".  Option letters are case sensitive.
//
//       getopt places in the external variable optind the argv index
//       of the next argument to be processed.  optind is initialized
//       to 0 before the first call to getopt.
//
//       When all options have been processed (i.e., up to the first
//       non-option argument), getopt returns EOF, optarg will point
//       to the argument, and optind will be set to the argv index of
//       the argument.  If there are no non-option arguments, optarg
//       will be set to NULL.
//
//       The special option "--" may be used to delimit the end of the
//       options;  EOF will be returned, and "--" (and everything after it)
//       will be skipped.
//
//  RETURN VALUE
//       For option letters contained in the string optstring, getopt
//       will return the option letter.  getopt returns a question mark (?)
//       when it encounters an option letter not included in optstring.
//       EOF is returned when processing is finished.
//
//  BUGS
//       1)  Long options are not supported.
//       2)  The GNU double-colon extension is not supported.
//       3)  The environment variable POSIXLY_CORRECT is not supported.
//       4)  The + syntax is not supported.
//       5)  The automatic permutation of arguments is not supported.
//       6)  This implementation of getopt() returns EOF if an error is
//           encountered, instead of -1 as the latest standard requires.
//
//  EXAMPLE
//       BOOL CMyApp::ProcessCommandLine(int argc, char *argv[])
//       {
//           int c;
//
//           while ((c = getopt(argc, argv, "aBn:")) != EOF)
//           {
//               switch (c)
//               {
//                   case 'a':
//                       TRACE(_T("option a\n"));
//                       //
//                       // set some flag here
//                       //
//                       break;
//
//                   case 'B':
//                       TRACE( _T("option B\n"));
//                       //
//                       // set some other flag here
//                       //
//                       break;
//
//                   case 'n':
//                       TRACE(_T("option n: value=%d\n"), atoi(optarg));
//                       //
//                       // do something with value here
//                       //
//                       break;
//
//                   case '?':
//                       TRACE(_T("ERROR: illegal option %s\n"), argv[optind-1]);
//                       return FALSE;
//                       break;
//
//                   default:
//                       TRACE(_T("WARNING: no handler for option %c\n"), c);
//                       return FALSE;
//                       break;
//               }
//           }
//           //
//           // check for non-option args here
//           //
//           return TRUE;
//       }
//
///////////////////////////////////////////////////////////////////////////////

char	*optarg;		// global argument pointer
int		optind = 0; 	// global argv index

int getopt(int argc, char* const* argv, char *optstring)
{
	static char *next = NULL;
	char c, *cp;
	if (optind == 0)
		next = NULL;

	optarg = NULL;

	if (next == NULL || *next == '\0')
	{
		if (optind == 0)
			optind++;

		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
		{
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		if (strcmp(argv[optind], "--") == 0)
		{
			optind++;
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		next = argv[optind]+1;
		optind++;
	}

	c = *next++;
	cp = strchr(optstring, c);

	if (cp == NULL || c == ':')
		return '?';

	cp++;
	if (*cp == ':')
	{
		if (*next != '\0')
		{
			optarg = next;
			next = NULL;
		}
		else if (optind < argc)
		{
			optarg = argv[optind];
			optind++;
		}
		else
		{
			return '?';
		}
	}

	return c;
}

#endif	/* !__MINGW32__ */
