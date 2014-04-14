/*
    Copyright (C) 1998 by Jorrit Tyberghein and Steve Israelson

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CS_SCANSTR_H__
#define __CS_SCANSTR_H__

#include "csextern.h"

/**\file
 * String scanning (\c sscanf() flavor).
 */

/**
 * Own version of \c sscanf that is a bit more relaxed towards spaces
 * and also accepts quoted strings (the quotes will not be included into
 * the result string).
 * 
 * \par Safe string arguments
 * The string formats \c %%s and \c %%S expect a pointer to a character array
 * as the destination value; the scanned string is then copied to that array.
 * As there is no way for \c csScanStr to know the size of the destination
 * buffer it might be overflowed, causing memory corruption.<br>
 * However, \c csScanStr supports the safe formats \c %%as and \c %%aS; they
 * behave like \c %%s and \c %%S, but expect a pointer <em>to a pointer</em>
 * as the destination. \c csScanStr will allocate a buffer (using cs_malloc)
 * of a sufficient size to hold the scanned string and store that pointer in
 * the destination.
 * 
 * Example:
 * \code
 * char* scanned_str (nullptr);
 * csScanStr (input, "%as", &scanned_str);
 * if (scanned_str)
 * {
 *   // Do stuff
 * }
 * cs_free (scanned_str);
 * \endcode
 * 
 * \par Supported format commands
 * - \c %%d -- integer number
 * - \c %%f -- floating point
 * - \c %%b -- boolean (0, 1, true, false, yes, no, on, off)
 * - \c %%s -- string (with or without single quotes)
 * - \c %%S -- string (delimited with double quotes)<br>
 *              \c \\n will be converted to a newline<br>
 *              \c \\t will be converted to a tab<br>
 *              \c \\\\ produces a \c \\ <br>
 *              \c \\" produces a \c " <br>
 *              all other combinations of \c \\ are copied.
 * - \c %%D -- list of integers, first argument should be a
 *		pointer to an array of integers, second argument
 *		a pointer to an integer which will contain the
 *		number of elements inserted in the list.
 * - \c %%F -- similarly, a list of floats.
 * - \c %%n -- this returns the amount of the input string
 *              thats been consumed, in characters. Does NOT
 *              increment the return count and does not read
 *              from the input string.
 *
 * Returns the number of successfully scanned arguments.
 * Hence if there is a mismatch effectively the number of the last
 * successfully scanned argument is returned.
 */
CS_CRYSTALSPACE_EXPORT int csScanStr (const char* in, const char* format, ...);

#endif // __CS_SCANSTR_H__
