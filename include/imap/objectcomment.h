/*
    Copyright (C) 2012 by Jorrit Tyberghein

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

#ifndef __CS_IMAP_IOBJECTCOMMENT_H__
#define __CS_IMAP_IOBJECTCOMMENT_H__

/**\file
 * Saving multiple files.
 */

#include "csutil/scf.h"

struct iObject;

/**
 * This interface represents a comment that is associated with an object in XML.
 * When the engine saveable flag is set then (some) of the loader and saver
 * plugins will respect this comment and try to load/save it as an XML comment
 * together with the object.
 */
struct iObjectComment : public virtual iBase
{
  SCF_INTERFACE (iObjectComment, 0, 0, 1);
  
  /**
   * Get the comment string.
   */
  virtual iString* GetComment () = 0;

  /**
   * Get the iObject for this object.
   */
  virtual iObject* QueryObject () = 0;
}; 

#endif // __CS_IMAP_IOBJECTCOMMENT_H__

