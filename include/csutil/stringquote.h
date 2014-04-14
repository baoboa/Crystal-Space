/*
    Copyright (C) 2010 by Frank Richter

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

#ifndef __CS_STRINGQUOTE_H__
#define __CS_STRINGQUOTE_H__

/**\file
 * Helper functions to decorate strings with nice-looking quote characters
 */

#include "csextern.h"
#include "csstring.h"

namespace CS
{
  /**
   * Helper functions to decorate strings with nice-looking quote characters.
   * \remarks These functions work upon UTF-8 strings. Non-UTF-8 input will
   *   likely be mangled.
   */
  struct CS_CRYSTALSPACE_EXPORT Quote
  {
    /**
     * Put single quotes (<tt>&lsquo;</tt>, <tt>&rsquo;</tt>) around a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void Single (csStringBase& out, const char* str);
    /**
     * Put single quotes (<tt>&lsquo;</tt>, <tt>&rsquo;</tt>) around a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* Single (const char* str);
    
    /**
     * Put a single left quote (<tt>&lsquo;</tt>) before a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void SingleLeft (csStringBase& out, const char* str = "");
    /**
     * Put a single left quote (<tt>&lsquo;</tt>) before a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* SingleLeft (const char* str = "");
    
    /**
     * Put a single right quote (<tt>&lsquo;</tt>) after a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void SingleRight (csStringBase& out, const char* str = "");
    /**
     * Put a single right quote (<tt>&lsquo;</tt>) after a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* SingleRight (const char* str = "");
    
    /**
     * Put double quotes (<tt>&ldquo;</tt>, <tt>&rdquo;</tt>) around a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void Double (csStringBase& out, const char* str);
    /**
     * Put double quotes (<tt>&ldquo;</tt>, <tt>&rdquo;</tt>) around a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* Double (const char* str);

    /**
     * Put a double left quote (<tt>&rdquo;</tt>) before a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void DoubleLeft (csStringBase& out, const char* str = "");
    /**
     * Put a double left quote (<tt>&rdquo;</tt>) before a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* DoubleLeft (const char* str = "");

    /**
     * Put a double right quote (<tt>&rdquo;</tt>) after a string.
     * \param out String to receive quoted input.
     * \param str String to quote.
     */
    static void DoubleRight (csStringBase& out, const char* str = "");
    /**
     * Put a double right quote (<tt>&rdquo;</tt>) after a string.
     * \param str String to quote.
     * \return Pointer to quoted input. The returned string will be discarded
     *   overwritten after a small, but indeterminate time. It is safe to
     *   assume it survives to be used as an argument to a function call, but
     *	 for anything longer than that the string should be stowed away
     *   manually somewhere.
     */
    static const char* DoubleRight (const char* str = "");
  };
} // namespace CS

#endif // __CS_STRINGQUOTE_H__
