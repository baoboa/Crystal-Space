/*
  Copyright (C) 2012 by Stefano Angeleri
            (C) 2011 by Frank Richter

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

#include "csutil/syspath.h"

#include "csutil/csuctransform.h"

#include <sys/types.h>
#include <sys/stat.h>

static inline int CS_stat (const char* path, struct stat* buf)
{
#if defined (__CYGWIN32__)
  return stat(path, buf);
#elif defined(CS_COMPILER_MSVC)
  size_t pathLen (strlen (path));
  size_t pathWlen (pathLen + 1);
  CS_ALLOC_STACK_ARRAY(wchar_t, pathW, pathWlen);
  csUnicodeTransform::UTF8toWC (pathW, pathWlen,
                                (utf8_char*)path, pathLen);
  /* Note: the cast works as struct stat and struct _stat64i32 effectively
     have the same layout */
  return _wstat64i32 (pathW, reinterpret_cast<struct _stat64i32*> (buf));
#else
  size_t pathLen (strlen (path));
  size_t pathWlen (pathLen + 1);
  CS_ALLOC_STACK_ARRAY(wchar_t, pathW, pathWlen);
  csUnicodeTransform::UTF8toWC (pathW, pathWlen,
                                (utf8_char*)path, pathLen);
  /* Note: the cast works as struct stat and struct _stat64i32 effectively
     have the same layout */
  return _wstat (pathW, reinterpret_cast<struct _stat*> (buf));
#endif
}

namespace CS
{
  namespace Platform
  {
    bool IsRegularFile (struct stat* file_stat)
    {
      return (file_stat->st_mode & _S_IFREG) != 0;
    }

    bool IsDirectory (struct stat* file_stat)
    {
      return (file_stat->st_mode & _S_IFDIR) != 0;
    }

    int Stat (const char* path, struct stat* buf)
    {
      int olderrno (errno);
      int result (0);
      if (CS_stat (path, buf) < 0)
      {
        result = errno;
      }
      errno = olderrno;
      return result;
    }
  } // namespace Platform
} // namespace CS
