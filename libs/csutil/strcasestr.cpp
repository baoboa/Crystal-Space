/*
    Copyright (C) 2012 by Stefano Angeleri.

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

#include "cssysdef.h"
#include <string.h>
#include <ctype.h>
#include "csutil/util.h"

namespace CS
{

const char* StrCaseStr(char const* str1, char const* str2)
{
    char const* str1Pos = str1;

    // If the string to be matched is empty match the start of the string
    // (similarly to strcasestr behaviour).
    if(!*str2)
    {
        return str1;
    }

    while(*str1Pos)
    {
      // Check if the first character of the compared string is at the current
      // position.
      if(tolower((unsigned char)*str1Pos) == tolower((unsigned char)*str2))
      {
        // In this case we start checking each character one by one with the
        // hope to match all.
        char const* str1MatchPos = str1Pos + 1;
        char const* str2MatchPos = str2 + 1;

        while(*str1MatchPos &&
              tolower((unsigned char)*str1MatchPos) == tolower((unsigned char)*str2MatchPos))
        {
          // Proceed to the next character.
          str1MatchPos++;
          str2MatchPos++;

          // If the second string has ended it means we found the point
          // we were looking for, so return it.
          if(!*str2MatchPos)
          {
            return str1Pos;
          }
        }
      }

      // The string didn't match so proceed to the next character of the
      // string we are searching into and try again.
      str1Pos++;
    }

    return nullptr;
}

} // namespace CS
