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

#ifndef __CS_CSTOOL_OBJECTCOMMENT_H__
#define __CS_CSTOOL_OBJECTCOMMENT_H__

/**\file
 * Saving multiple files.
 */

#include "csutil/scf.h"
#include "csutil/scfstr.h"
#include "csutil/csobject.h"
#include "iutil/document.h"
#include "iutil/objreg.h"
#include "imap/objectcomment.h"
#include "iengine/engine.h"

namespace CS
{
namespace Persistence
{

/**
 * Default implementation of iObjectComment.
 */
class ObjectComment : public scfImplementationExt1<
					     ObjectComment,csObject,iObjectComment>
{
private:
  csRef<scfString> comment;

public:
  ObjectComment () : scfImplementationType (this)
  {
    comment.AttachNew (new scfString ());
  }
  virtual ~ObjectComment () { }

  virtual iString* GetComment () { return comment; }
  virtual iObject* QueryObject () { return (csObject*)this; }
}; 

/**
 * Helper function to save a comment.
 * Returns true if it was actually saved.
 * This function will not save if the engine saveable flag is not set.
 */
inline bool SaveComment (iEngine* engine, iObject* obj, iDocumentNode* parentNode)
{
  if (!engine->GetSaveableFlag ()) return false;
  csRef<iObjectComment> comment = CS::GetChildObject<iObjectComment> (obj);
  if (comment)
  {
    csRef<iDocumentNode> commentNode = parentNode->CreateNodeBefore (CS_NODE_COMMENT, 0);
    commentNode->SetValue (comment->GetComment ()->GetData ());
    return true;
  }
  return false;
}

/**
 * Helper function to save a comment.
 * Returns true if it was actually saved.
 * This function will not save if the engine saveable flag is not set.
 */
inline bool SaveComment (iObjectRegistry* object_reg, iObject* obj, iDocumentNode* parentNode)
{
  csRef<iEngine> engine = csQueryRegistry<iEngine> (object_reg);
  return SaveComment (engine, obj, parentNode);
}

/**
 * Handle a comment node for a given object. If the engine saveable
 * flag is on then an iObjectComment is created and add to the object.
 * Otherwise nothing happens.
 * This function returns true if a comment was added. False otherwise.
 * If 'replace' is true then previously existing comments (if any) will be
 * overwritten by the new comment.
 */
inline bool LoadComment (iEngine* engine, iObject* object, iDocumentNode* node,
		  bool replace = false)
{
  if (!engine) return false;
  if (!engine->GetSaveableFlag ()) return false;
  csRef<iObjectComment> comment = CS::GetChildObject<iObjectComment> (object);
  if (comment && replace)
  {
    comment->GetComment ()->Replace (node->GetValue ());
    return true;
  }
  else if (!comment)
  {
    comment.AttachNew (new CS::Persistence::ObjectComment ());
    object->ObjAdd (comment->QueryObject ());
    comment->GetComment ()->Replace (node->GetValue ());
    return true;
  }
  return false;
}

/**
 * Handle a comment node for a given object. If the engine saveable
 * flag is on then an iObjectComment is created and add to the object.
 * Otherwise nothing happens.
 * This function returns true if a comment was added. False otherwise.
 * If 'replace' is true then previously existing comments (if any) will be
 * overwritten by the new comment.
 */
inline bool LoadComment (iObjectRegistry* object_reg, iObject* object,
		iDocumentNode* node, bool replace = false)
{
  csRef<iEngine> engine = csQueryRegistry<iEngine> (object_reg);
  return LoadComment (engine, object, node, replace);
}

}
}

#endif // __CS_CSTOOL_OBJECTCOMMENT_H__

