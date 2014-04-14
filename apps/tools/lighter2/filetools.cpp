/*
  Copyright (C) 2005 by Marten Svanfeldt
            (C) 2012 by Frank Richter

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

#include "common.h"

#include "filetools.h"

namespace lighter
{
  /**
   * Sanitize some asset name so it's a suitable for a filename
   * (no embedded slashes etc.)
   */
  csString MakeFilename (const char* name)
  {
    csString out (name);
    out.ReplaceAll ("\\", "_"); //replace bad characters
    out.ReplaceAll ("/", "_");
    out.ReplaceAll (" ", "_");
    out.ReplaceAll (".", "_");
    return out;
  }
} // namespace lighter
