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

#include "cssysdef.h"
#include "csutil/stringquote.h"

#include "csutil/threading/tls.h"

namespace CS
{
  // Double quotes, UTF-8
  #define LDQUO   "\xe2\x80\x9c"
  #define RDQUO   "\xe2\x80\x9d"
  // Single quotes, UTF-8
  #define LSQUO   "\xe2\x80\x98"
  #define RSQUO   "\xe2\x80\x99"

  /* Number of strings buffer used to store return values:
     A returned string gets invalid after numString calls to quote functions. */
  enum { numStrings = 16 };
  
  namespace
  {
    struct QuoteStrings
    {
      int n;
      csStringFast<128> strings[numStrings];
      
      QuoteStrings() : n (0) {}
    };

    CS_IMPLEMENT_STATIC_VAR(GetStrings, CS::Threading::ThreadLocal<QuoteStrings>, );
  } // anonymous namespace
  
  // Helper function to get a string to store returned strings
  static csStringBase& GetReturnString()
  {
    QuoteStrings& retStrings = *(GetStrings());
    csStringBase& outStr = retStrings.strings[retStrings.n];
    retStrings.n = (retStrings.n + 1) % numStrings;
    return outStr;
  }
  
  void Quote::Single (csStringBase& out, const char* str)
  {
    out.Replace (LSQUO);
    out.Append (str);
    out.Append (RSQUO);
  }
  
  const char* Quote::Single (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    Single (outStr, str);
    return outStr;
  }
  
  void Quote::SingleLeft (csStringBase& out, const char* str)
  {
    out.Replace (LSQUO);
    out.Append (str);
  }
  
  const char* Quote::SingleLeft (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    SingleLeft (outStr, str);
    return outStr;
  }
  
  void Quote::SingleRight (csStringBase& out, const char* str)
  {
    out.Replace (str);
    out.Append (RSQUO);
  }
  
  const char* Quote::SingleRight (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    SingleRight (outStr, str);
    return outStr;
  }
  
  void Quote::Double (csStringBase& out, const char* str)
  {
    out.Replace (LDQUO);
    out.Append (str);
    out.Append (RDQUO);
  }
  
  const char* Quote::Double (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    Double (outStr, str);
    return outStr;
  }
  
  void Quote::DoubleLeft (csStringBase& out, const char* str)
  {
    out.Replace (LDQUO);
    out.Append (str);
  }
  
  const char* Quote::DoubleLeft (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    DoubleLeft (outStr, str);
    return outStr;
  }
  
  void Quote::DoubleRight (csStringBase& out, const char* str)
  {
    out.Replace (str);
    out.Append (RDQUO);
  }
  
  const char* Quote::DoubleRight (const char* str)
  {
    csStringBase& outStr (GetReturnString());
    DoubleRight (outStr, str);
    return outStr;
  }
  
} // namespace CS
