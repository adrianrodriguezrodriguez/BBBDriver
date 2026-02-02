//=============================================================================
// Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights 
// Reserved
// 
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the non-disclosure or license agreement you
// entered into with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

//=============================================================================
// $Id$
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
//
//  FILE: getopt.c
//
//      GetOption function
//
//  FUNCTIONS:
//
//      GetOption() - Get next command line option and parameter
//
//  COMMENTS:
//   Codes taken from MFC example and slightly modified to make it Unix compatible.
//
///////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include "Getopt.h"

///////////////////////////////////////////////////////////////////////////////
//
//  FUNCTION: GetOption()
//
//      Get next command line option and parameter
//
//  PARAMETERS:
//
//      argc - count of command line arguments
//      argv - array of command line argument strings
//      pszValidOpts - string of valid, case-sensitive option characters,
//                     a colon ':' following a given character means that
//                     option can take a parameter
//      ppszParam - pointer to a pointer to a string for output
//
//  USAGE:
//    Command line options are expected to be composed of a series of option flags,
//    each preceded by the special option flag delimiter '-', and each may be
//    optionally followed by an option arguments.
//
//    eg. -A 1 -B 2 -C -D hello
//
//    In the above example, we have four option flags, A, B, C, D.  A has an argument
//    value of 1, B has an argument value of 2, C has no arugment, and D has an
//    argument string of hello.
//
//    In order to parse the above correctly, the pszValidOpts string is expected to be
//
//    A:B:CD:
//
//    Indicating that again there are four possible option flags, A, B, and D will be
//    followed by an argument each, whereas C will have no arugment.
//
//  RETURNS:
//
//      If valid option is found, the character value of that option
//          is returned, and *ppszParam points to the parameter if given,
//          or is NULL if no param
//      If standalone parameter (with no option) is found, 1 is returned,
//          and *ppszParam points to the standalone parameter
//      If option is found, but it is not in the list of valid options,
//          -1 is returned, and *ppszParam points to the invalid argument
//      When end of argument list is reached, 0 is returned, and
//          *ppszParam is NULL
//
//  COMMENTS:
//    1. Negative arguments must be preceded by an additional '-' to distinguish them
//    from regular option flags.
//    2. Option flags are preceded by '-' only, so as to avoid confusion with
//    path name specification under Unix.
//
///////////////////////////////////////////////////////////////////////////////

int GetOption (
    int argc,
    char** argv,
    const char* pszValidOpts,
    const char** ppszParam)
{
    static int iArg = 1;
    char chOpt;
    const char* psz = NULL;
    const char* pszParam = NULL;

    if (iArg < argc)
    {
        psz = &(argv[iArg][0]);
#if 0
        if (*psz == '-' || *psz == '/')
#else
        if (*psz == '-' )
#endif
        {
            // we have an option specifier
            chOpt = argv[iArg][1];
            if (isalnum(chOpt) || ispunct(chOpt))
            {
                // we have an option character
                psz = strchr(pszValidOpts, chOpt);
                if (psz != NULL)
                {
                    // option is valid, we want to return chOpt
                    if (psz[1] == ':')
                    {
                        // option can have a parameter
                        psz = &(argv[iArg][2]);
                        if (*psz == '\0')
                        {
                            // must look at next argv for param
                            if (iArg+1 < argc)
                            {
                                psz = &(argv[iArg+1][0]);
#if 0
                                if (*psz == '-' || *psz == '/')
#else
                                if (*psz == '-' )
#endif
                                {
                                   if( psz[1] == '-' )
                                   {
                                      // special case : -- to denote escape the negative as option
                                      iArg++;
                                      pszParam = &psz[1];
                                   }
                                   // otherwise do nothing to indicate that
                                    // next argv is a new option, so param
                                    // not given for current option
                                }
                                else
                                {
                                    // next argv is the param
                                    iArg++;
                                    pszParam = psz;
                                }
                            }
                            else
                            {
                                // reached end of args looking for param
                            }

                        }
                        else
                        {
                            // param is attached to option
                            pszParam = psz;
                        }
                    }
                    else
                    {
                        // option is alone, has no parameter
                    }
                }
                else
                {
                    // option specified is not in list of valid options
                    chOpt = -1;
                    pszParam = &(argv[iArg][0]);
                }
            }
            else
            {
                // though option specifier was given, option character
                // is not alpha or was was not specified
                chOpt = -1;
                pszParam = &(argv[iArg][0]);
            }
        }
        else
        {
            // standalone arg given with no option specifier
            chOpt = 1;
            pszParam = &(argv[iArg][0]);
        }
    }
    else
    {
        // end of argument list
        chOpt = 0;
    }

    iArg++;
    *ppszParam = pszParam;
    return (chOpt);
}
