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


#include <sys/types.h>
#include <sys/stat.h>

#include "cssysdef.h"
#include "csutil/syspath.h"

namespace CS
{
  namespace Platform
  {
    bool IsRegularFile (struct stat* file_stat)
    {
      return file_stat->st_mode & S_IFREG;
    }
      
    bool IsDirectory (struct stat* file_stat)
    {
      return file_stat->st_mode & S_IFDIR;
    }
      
    int Stat (const char* path, struct stat* buf)
    {
      int olderrno (errno);
      int result (0);
      if (stat (path, buf) < 0)
      {
        result = errno;
      }
      errno = olderrno;
      return result;
    }
  } // namespace Platform
} // namespace CS
