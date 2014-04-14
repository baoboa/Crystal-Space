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

#include "cssysdef.h"
#include <string.h>
#include "csutil/sysfunc.h"
#include "csutil/event.h"
#include "csutil/eventnames.h"
#include "csutil/eventhandlers.h"
#include "blobs.h"
#include "iutil/objreg.h"
#include "iutil/object.h"
#include "iutil/event.h"
#include "iutil/eventq.h"
#include "iutil/virtclk.h"
#include "iutil/vfs.h"
#include "ivideo/graph3d.h"
#include "ivideo/graph2d.h"
#include "ivideo/texture.h"
#include "imap/loader.h"
#include "iengine/texture.h"

//CS_IMPLEMENT_PLUGIN

SCF_IMPLEMENT_FACTORY (csBlobManager)

//--------------------------------------------------------------------------

bool csLineBlobMover::Update (csTicks t, iMovingObject* blob)
{
  currenttime += float (t);
  float d;
  if (currenttime >= totaltime) d = 1.0f;
  else d = 1.0f - (totaltime-currenttime) / totaltime;
  int nx = int (float (x1) + float (x2-x1) * d);
  int ny = int (float (y1) + float (y2-y1) * d);
  blob->Move (nx, ny);
  time_already_taken = currenttime - totaltime;
  return currenttime >= totaltime;
}

//--------------------------------------------------------------------------

void csGeomLine::Draw (csBlobManager* mgr, int px, int py)
{
  mgr->G2D->DrawLine (px + x1, py + y1, px + x2, py + y2, color);
}

void csGeomBox::Draw (csBlobManager* mgr, int px, int py)
{
  mgr->G2D->DrawBox (px+x, py+y, w, h, color);
}

void csGeomText::ChangeText (const char* t)
{
  text.Empty ();
  text.SplitString (t, "\n");
}

void csGeomText::Draw (csBlobManager* mgr, int px, int py)
{
  size_t i;
  int yy = py + y;
  for (i = 0 ; i < text.GetSize () ; i++)
  {
    mgr->G2D->Write (mgr->fnt, px+x, yy, fg, bg, (const char*)(text[i]));
    yy = yy + mgr->fnt_height+2;
  }
}

csGeomBlob::csGeomBlob (csBlobManager* mgr, int x, int y, const char* name)
    : x (x), y (y), name (name)
{
  blob = new csBlob (mgr, name, 0 /*csBlobManager::LAYER_UI*/, 0, 0);
  blob->Move (x, y);
}

csGeomBlob::~csGeomBlob ()
{
  delete blob;
}

void csGeomBlob::Draw (csBlobManager*, int px, int py)
{
  blob->Move (blob->GetX ()+px, blob->GetY ()+py);
  blob->Draw ();
  blob->Move (blob->GetX ()-px, blob->GetY ()-py);
}

void csGeomBlob::ChangeBlobImage (const char* name)
{
  csGeomBlob::name = name;
  blob->AddImage ("__main__", name);
}

void csGeomBlob::ScaleGeom (int w, int h)
{
  blob->Scale (w, h);
}

int csGeomBlob::GetGeomWidth ()
{
  return blob->GetWidth ();
}

int csGeomBlob::GetGeomHeight ()
{
  return blob->GetHeight ();
}

//--------------------------------------------------------------------------

static uint32 global_id = 0;

csGeom::csGeom (csBlobManager* mgr, int layer, int w, int h)
  : scfImplementationType (this), mgr (mgr), layer (layer),
    w (w), h (h)
{
  viewport = 0;
  x = 0;
  y = 0;
  hidden = false;
  clickable = true;
  mover = 0;
  gid = 0;
  id = global_id++;
}

csGeom::~csGeom ()
{
  delete mover;
}

void csGeom::SetViewPort (iBlobViewPort* viewport)
{
  if (csGeom::viewport) csGeom::viewport->RemoveMovingObject (
      static_cast<iMovingObject*> (this));
  csGeom::viewport = static_cast<csBlobViewPort*> (viewport);
  if (csGeom::viewport) csGeom::viewport->AddMovingObject (
      static_cast<iMovingObject*> (this));
}

void csGeom::Move (int x, int y)
{
  if (csGeom::x == x && csGeom::y == y) return;
  csGeom::x = x;
  csGeom::y = y;
}

bool csGeom::In (int x, int y)
{
  return (x >= csGeom::x && x < (csGeom::x+w) && y >= csGeom::y && y < (csGeom::y+h));
}

void csGeom::SetLineMover (int x1, int y1, int x2, int y2, csTicks totaltime)
{
  if (mover) delete mover;
  mover = new csLineBlobMover (x1, y1, x2, y2, totaltime);
}

void csGeom::SetPathMover (int start_x, int start_y)
{
  if (mover) delete mover;
  mover = new csPathBlobMover (start_x, start_y);
}

void csGeom::AddPathSegment (int x, int y, csTicks segmenttime)
{
  if (!mover) return;
  csPathBlobMover* pmover = (csPathBlobMover*)mover;
  pmover->AddSegment (x, y, segmenttime);
}

uint32 csGeom::Box (int x, int y, int w, int h, int color)
{
  csGeomBox* b = new csGeomBox (x, y, w, h, color);
  gid++;
  geometries_hash.Put (gid, b);
  geometries.Push (b);
  return gid;
}

uint32 csGeom::Line (int x1, int y1, int x2, int y2, int color)
{
  csGeomLine* b = new csGeomLine (x1, y1, x2, y2, color);
  gid++;
  geometries_hash.Put (gid, b);
  geometries.Push (b);
  return gid;
}

uint32 csGeom::Text (int x, int y, int fg, int bg, const char* text)
{
  csGeomText* b = new csGeomText (x, y, fg, bg, text);
  gid++;
  geometries_hash.Put (gid, b);
  geometries.Push (b);
  return gid;
}

uint32 csGeom::Blob (int x, int y, const char* name)
{
  csGeomBlob* b = new csGeomBlob (mgr, x, y, name);
  gid++;
  geometries_hash.Put (gid, b);
  geometries.Push (b);
  return gid;
}

void csGeom::Remove (uint32 gid)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
  {
    geometries_hash.DeleteAll (gid);
    geometries.Delete (o);
  }
}

void csGeom::Update (csTicks elapsed)
{
  if (mover)
  {
    if (mover->Update (elapsed, this))
    {
      delete mover;
      mover = 0;
    }
  }
}

bool csGeom::GetRealPosInternal (int& px, int& py)
{
  if (viewport == 0)
  {
    px = x;
    py = y;
    return true;
  }
  else
  {
    if (x+w-1 < viewport->scrollx) return false;
    if (y+h-1 < viewport->scrolly) return false;
    px = x + viewport->x1 - viewport->scrollx;
    if (px >= viewport->x2) return false;
    py = y + viewport->y1 - viewport->scrolly;
    if (py >= viewport->y2) return false;
    return true;
  }
}

int csGeom::GetRealX ()
{
  if (viewport == 0) return x;
  else return x + viewport->x1 - viewport->scrollx;
}

int csGeom::GetRealY ()
{
  if (viewport == 0) return y;
  else return y + viewport->y1 - viewport->scrolly;
}

void csGeom::Draw ()
{
  if (hidden) return;
  int px, py;
  if (!GetRealPosInternal (px, py)) return;
  size_t i;
  for (i = 0 ; i < geometries.GetSize () ; i++)
  {
    csGeomObject* obj = geometries[i];
    obj->Draw (mgr, px, py);
  }
}

void csGeom::ChangeColor (uint32 gid, int color)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
  {
    o->ChangeColor (color);
  }
}

void csGeom::ChangeText (uint32 gid, const char* text)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
  {
    o->ChangeText (text);
  }
}

void csGeom::ChangeBlobImage (uint32 gid, const char* name)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
  {
    o->ChangeBlobImage (name);
  }
}

void csGeom::ScaleGeom (uint32 gid, int w, int h)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
  {
    o->ScaleGeom (w, h);
  }
}

int csGeom::GetGeomWidth (uint32 id)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
    return o->GetGeomWidth ();
  return 0;
}

int csGeom::GetGeomHeight (uint32 id)
{
  csGeomObject* o = geometries_hash.Get (gid, 0);
  if (o)
    return o->GetGeomHeight ();
  return 0;
}

//--------------------------------------------------------------------------

// @@@ Optimize usage of __main__!!!

csBlob::csBlob (csBlobManager* mgr, const char* name, int layer,
    const char* filename, iImageModifier* modifier)
    : scfImplementationType (this), mgr (mgr), name (name),
      modifier (modifier), layer (layer)
{
  viewport = 0;
  x = 0;
  y = 0;
  hidden = false;
  clickable = true;
  alpha = 0.0f;
  ualpha = 0;
  mover = 0;
  csBlobImage* bi = FindOrCreateBlobImage (name, filename, modifier);
  bi->GetDimensions (w, h);
  orig_w = w;
  orig_h = h;
  bi_list.Push (csBlobImageInstance ("__main__", bi, false));

  curanim = 0;
  curanim_loop = false;
  curanim_remain = false;
  curanim_origimg = name;

  destalpha_ticks = 0;
  id = global_id++;
}

csBlob::~csBlob ()
{
  delete mover;
  csHash<csBlobAnimation*,csString>::GlobalIterator it = animations.GetIterator ();
  while (it.HasNext ())
  {
    csBlobAnimation* anim = it.Next ();
    delete anim;
  }
}

void csBlob::SetViewPort (iBlobViewPort* viewport)
{
  if (csBlob::viewport) csBlob::viewport->RemoveMovingObject (
      static_cast<iMovingObject*> (this));
  csBlob::viewport = static_cast<csBlobViewPort*> (viewport);
  if (csBlob::viewport) csBlob::viewport->AddMovingObject (
      static_cast<iMovingObject*> (this));
}

size_t csBlob::FindTaggedIndex (const char* tag)
{
  size_t i;
  for (i = 0 ; i < bi_list.GetSize () ; i++)
    if (bi_list[i].tag == tag)
      return i;
  return csArrayItemNotFound;
}

void csBlob::RemoveSecondaryImages ()
{
  size_t i = FindTaggedIndex ("__main__");
  csBlobImageInstance bi = bi_list[i];
  bi_list.Empty ();
  bi_list.Push (bi);
}

void csBlob::AddImage (const char* tag, const char* name,
      size_t index, int offsetx, int offsety, int w, int h)
{
  if (index > bi_list.GetSize ()) index = bi_list.GetSize ();
  csBlobImage* bi = FindOrCreateBlobImage (name, 0, modifier);
  size_t i = FindTaggedIndex (tag);
  if (i == csArrayItemNotFound)
  {
    bi_list.Insert (index, csBlobImageInstance (tag, bi, false));
    i = index;
  }
  else
  {
    bi_list[i].bi = bi;
  }
  bi_list[i].offsetx = offsetx;
  bi_list[i].offsety = offsety;
  bi_list[i].w = w;
  bi_list[i].h = h;
}

csBlobImage* csBlob::FindOrCreateBlobImage (const char* name, const char* filename,
      iImageModifier* modifier)
{
  csBlobImage* bi = static_cast<csBlobImage*> (mgr->GetOrCreateBlobImage (name, modifier));
  if (bi == 0)
  {
    if (filename == 0 || *filename == 0)
    {
      printf ("Cannot find image with name '%s'!\n", name);
      fflush (stdout);
      return 0;
    }
    bi = static_cast<csBlobImage*> (mgr->CreateBlobImage (name, modifier, filename));
  }
  return bi;
}

void csBlob::ClearModifier ()
{
  if (modifier == 0) return;
  modifier = 0;
  size_t i;
  for (i = 0 ; i < bi_list.GetSize () ; i++)
  {
    if (bi_list[i].bi->HasModifier ())
    {
      csBlobImage* bi = FindOrCreateBlobImage (bi_list[i].bi->GetName (), 0, 0);
      bi_list[i].bi = bi;
    }
  }
}

void csBlob::SetModifier (iImageModifier* modifier)
{
  if (csBlob::modifier == modifier) return;
  ClearModifier ();
  csBlob::modifier = modifier;
  size_t i;
  for (i = 0 ; i < bi_list.GetSize () ; i++)
  {
    csBlobImage* bi = FindOrCreateBlobImage (bi_list[i].bi->GetName (), 0, modifier);
    bi_list[i].bi = bi;
  }
}

void csBlob::SetLineMover (int x1, int y1, int x2, int y2, csTicks totaltime)
{
  if (mover) delete mover;
  mover = new csLineBlobMover (x1, y1, x2, y2, totaltime);
}

void csBlob::SetPathMover (int start_x, int start_y)
{
  if (mover) delete mover;
  mover = new csPathBlobMover (start_x, start_y);
}

void csBlob::AddPathSegment (int x, int y, csTicks segmenttime)
{
  if (!mover) return;
  csPathBlobMover* pmover = (csPathBlobMover*)mover;
  pmover->AddSegment (x, y, segmenttime);
}

void csBlob::Update (csTicks elapsed)
{
  if (mover)
  {
    if (mover->Update (elapsed, this))
    {
      delete mover;
      mover = 0;
    }
  }

  if (destalpha_ticks > 0)
  {
    destalpha_ticks -= elapsed;
    if (destalpha_ticks <= 0)
    {
      destalpha_ticks = 0;
      alpha = destalpha;
    }
    else
    {
      alpha = destalpha + float (destalpha_ticks)
        * destalpha_factor * (srcalpha - destalpha);
    }
    int a = int (alpha * 255.0);
    if (a > 255) a = 255;
    ualpha = uint8 (a);
  }

  if (curanim)
  {
    bool changed = false;
    curanim_timeleft -= elapsed;
    while (curanim_index == -1 || curanim_timeleft <= 0)
    {
      changed = true;
      curanim_index++;
      if (curanim_index >= int (curanim->images.GetSize ()))
      {
	if (curanim_loop)
	  curanim_index = 0;
        else
	{
	  StopAnimations ();
	  return;
	}
      }
      curanim_timeleft += anim_delay;
    }
    if (changed)
    {
      AddImage ("__main__", curanim->images[curanim_index]);
    }
  }
}

bool csBlob::GetRealPosInternal (int& px, int& py)
{
  if (viewport == 0)
  {
    px = x;
    py = y;
    return true;
  }
  else
  {
    if (x+w-1 < viewport->scrollx) return false;
    if (y+h-1 < viewport->scrolly) return false;
    px = x + viewport->x1 - viewport->scrollx;
    if (px >= viewport->x2) return false;
    py = y + viewport->y1 - viewport->scrolly;
    if (py >= viewport->y2) return false;
    return true;
  }
}

int csBlob::GetRealX ()
{
  if (viewport == 0) return x;
  else return x + viewport->x1 - viewport->scrollx;
}

int csBlob::GetRealY ()
{
  if (viewport == 0) return y;
  else return y + viewport->y1 - viewport->scrolly;
}

void csBlob::Draw ()
{
  if (hidden) return;
  int px, py;
  if (!GetRealPosInternal (px, py)) return;
  size_t i;
  int ox, oy;
  for (i = 0 ; i < bi_list.GetSize () ; i++)
  {
    // @@@ Should not be needed!
    if (!bi_list[i].bi->GetTextureHandle ()) continue;
    bi_list[i].bi->GetOffset (ox, oy);
    //if (bi_list[i].tag == "__main__")
      //continue;
#if 0
    if (bi_list[i].w != 0)
    {
      ww = bi_list[i].w;
      orig_ww = ww;
    }
    else
    {
      ww = w;
      orig_ww = orig_w;
    }
    if (bi_list[i].h != 0)
    {
      hh = bi_list[i].h;
      orig_hh = hh;
    }
    else
    {
      hh = h;
      orig_hh = orig_h;
    }
#endif
    if (bi_list[i].swap)
      //mgr->G3D->DrawPixmap (bi_list[i].bi->GetTextureHandle (), px+bi_list[i].offsetx, py+bi_list[i].offsety, ww, hh,
	  //ox+ww, oy, -orig_ww, orig_hh, ualpha);
      mgr->G3D->DrawPixmap (bi_list[i].bi->GetTextureHandle (), px, py, w, h, ox+w, oy, -orig_w, orig_h, ualpha);
    else
      //mgr->G3D->DrawPixmap (bi_list[i].bi->GetTextureHandle (), px+bi_list[i].offsetx, py+bi_list[i].offsety, ww, hh,
	  //ox, oy, orig_ww, orig_hh, ualpha);
      mgr->G3D->DrawPixmap (bi_list[i].bi->GetTextureHandle (), px, py, w, h, ox, oy, orig_w, orig_h, ualpha);
  }
}

void csBlob::AnimateAlpha (float destalpha, csTicks ticks)
{
  srcalpha = alpha;
  csBlob::destalpha = destalpha;
  destalpha_ticks = ticks;
  destalpha_factor = 1.0f / float (ticks);
}

void csBlob::SetHorizontalSwap (bool sw, const char* tag)
{
  size_t i = FindTaggedIndex (tag);
  if (i == csArrayItemNotFound) return;
  bi_list[i].swap = sw;
}

void csBlob::AddAnimationImage (const char* animname, const char* image)
{
  csBlobAnimation* ba = animations.Get (animname, 0);
  if (!ba)
  {
    ba = new csBlobAnimation (animname);
    animations.Put (animname, ba);
  }
  ba->images.Push (image);
}

csTicks csBlob::PlayAnimation (const char* animname, bool loop,
      bool remain, csTicks delay)
{
  StopAnimations ();
  anim_delay = delay;
  curanim = animations.Get (animname, 0);
  if (!curanim) return 0;
  curanim_loop = loop;
  curanim_remain = remain;
  curanim_index = -1;
  curanim_timeleft = 0;
  AddImage ("__main__", curanim->images[0]);
  return delay * curanim->images.GetSize ();
}

void csBlob::StopAnimations ()
{
  curanim = 0;
  curanim_index = 0;
  curanim_timeleft = anim_delay;
  if (!curanim_remain)
    AddImage ("__main__", curanim_origimg);
}

void csBlob::Move (int x, int y)
{
  if (csBlob::x == x && csBlob::y == y) return;
  csBlob::x = x;
  csBlob::y = y;
}

bool csBlob::In (int x, int y)
{
  return (x >= csBlob::x && x < (csBlob::x+w) && y >= csBlob::y && y < (csBlob::y+h));
}

//--------------------------------------------------------------------------

csBlobImage::csBlobImage (csBlobManager* mgr,
    const char* name, iImageModifier* modifier, const char* filename)
    : scfImplementationType (this), mgr (mgr), name (name), modifier (modifier),
      filename (filename)
{
  csMapTexture* mt = mgr->mappedTextures.Get (name, 0);
  if (mt)
  {
    ox = mt->x;
    oy = mt->y;
    ow = mt->w;
    oh = mt->h;
    name = mt->big;
  }
  else
  {
    ox = oy = ow = oh = 0;
  }
  csString txtname = ConstructTxtName (name, modifier);
  txthandle = 0;
  txt = mgr->engine->FindTexture (txtname);
  if (txt) ;
  else if (modifier)
  {
    csString origname = ConstructTxtName (name, 0);
    txt = mgr->engine->FindTexture (origname);
    if (txt)
    {
      DoModifier ();
    }
    else
    {
      if (filename == 0 || *filename == 0)
      {
	printf ("Error finding texture '%s' (txtname='%s', origname='%s')!\n",
	    (const char*)name,
	    (const char*)txtname,
	    (const char*)origname);
	fflush (stdout);
	return; // Can't do anything.
      }
      txt = mgr->loader->LoadTexture (txtname, filename,
	  CS_TEXTURE_NOMIPMAPS+CS_TEXTURE_NOFILTER, mgr->G3D->GetTextureManager (),
	  true, true, false);
      DoModifier ();
    }
  }
  else
  {
    if (filename == 0 || *filename == 0)
    {
      printf ("Error finding texture '%s' (txtname='%s')!\n",
	    (const char*)name,
	    (const char*)txtname);
      fflush (stdout);
      return; // Can't do anything.
    }
    txt = mgr->loader->LoadTexture (txtname, filename,
	CS_TEXTURE_NOMIPMAPS+CS_TEXTURE_NOFILTER, mgr->G3D->GetTextureManager (),
	true, true, false);
    DoModifier ();
  }
  Setup ();
}

void csBlobImage::DoModifier ()
{
  if (!txt) return;
  csRef<iImage> image = txt->GetImageFile ();
  image = modifier->Modify (image);
  txt = mgr->engine->GetTextureList ()->NewTexture (image);
  csString n;
  csMapTexture* mt = mgr->mappedTextures.Get (name, 0);
  if (mt) n = mt->big;
  else n = name;
  txt->QueryObject ()->SetName (ConstructTxtName (n, modifier));
  txt->SetFlags (txt->GetFlags ()+CS_TEXTURE_NOMIPMAPS+CS_TEXTURE_NOFILTER);
  txt->Register (mgr->G3D->GetTextureManager ());
}

void csBlobImage::Setup ()
{
  if (!txt) { txthandle = 0; return; }
  txthandle = txt->GetTextureHandle ();
  txthandle->GetOriginalDimensions (txt_w, txt_h);
  txthandle->Precache ();
}

//--------------------------------------------------------------------------

bool csBlobViewPort::Update (csTicks current)
{
  if (scroll_total_time > 0)
  {
    float dt = float (current-scroll_start_time);
    if (dt > scroll_total_time)
    {
      bool updated = Scroll (int (end_scrollx), int (end_scrolly));
      scroll_total_time = 0;
      return updated;
    }
    else
    {
      float factor = dt / scroll_total_time;
      float sx = start_scrollx + factor * (end_scrollx - start_scrollx);
      float sy = start_scrolly + factor * (end_scrolly - start_scrolly);
      int x = scrollx;
      int y = scrolly;
      scrollx = int (sx);
      scrolly = int (sy);
      return x != scrollx || y != scrolly;
    }
  }
  return false;
}

void csBlobViewPort::ClearModifier ()
{
  size_t i;
  for (i = 0 ; i < objects.GetSize () ; i++)
  {
    csRef<iBlob> b = scfQueryInterface<iBlob> (objects[i]);
    if (b) b->ClearModifier ();
  }
}

void csBlobViewPort::SetModifier (iImageModifier* modifier, int layer)
{
  size_t i;
  for (i = 0 ; i < objects.GetSize () ; i++)
  {
    csRef<iBlob> b = scfQueryInterface<iBlob> (objects[i]);
    if (b)
    {
      if (layer == -1 || b->GetLayer () == layer)
        b->SetModifier (modifier);
    }
  }
}

bool csBlobViewPort::Scroll (int x, int y)
{
  bool updated = (scrollx != x || scrolly != y);
  scrollx = x;
  scrolly = y;
  scroll_total_time = -1.0f;
  return updated;
}

csTicks csBlobViewPort::InitiateScroll (int newx, int newy)
{
  if (newx == scrollx && newy == scrolly)
    return 0;
  start_scrollx = float (scrollx);
  start_scrolly = float (scrolly);
  end_scrollx = float (newx);
  end_scrolly = float (newy);
  float dx = end_scrollx - start_scrollx;
  float dy = end_scrolly - start_scrolly;
  float dist = sqrt (dx * dx + dy * dy);
  //scroll_start_time = float (mgr->vc->GetCurrentTicks ());
  scroll_start_time = float (csGetTicks ());
  if (dist <= 200)
    scroll_total_time = dist;
  else if (dist < 500)
    scroll_total_time = dist/2.0f;
  else
    scroll_total_time = dist/4.0f;
  scroll_total_time *= scrollspeed;
  return csTicks (scroll_total_time);
}

csTicks csBlobViewPort::ScrollRelative (int dx, int dy)
{
  return InitiateScroll (scrollx+dx, scrolly+dy);
}

csTicks csBlobViewPort::ScrollVisible (int x, int y, int margin)
{
  int w = x2-x1+1;
  int h = y2-y1+1;
  int sx = scrollx;
  int sy = scrolly;
  if (x < sx+margin) sx = x - margin;
  if (x > sx+w-margin) sx = x + margin-w;
  if (y < sy+margin) sy = y - margin;
  if (y > sy+h-margin) sy = y + margin-h;
  return InitiateScroll (sx, sy);
}

iMovingObject* csBlobViewPort::FindMovingObject (int x, int y)
{
  size_t i;
  x += scrollx;
  y += scrolly;
  for (i = 0 ; i < objects.GetSize () ; i++)
  {
    iMovingObject* g = objects[i];
    if (g->IsClickable () && g->In (x, y))
      return g;
  }
  return 0;
}

void csBlobViewPort::AddMovingObject (iMovingObject* mo)
{
  objects.Push (mo);
}

void csBlobViewPort::RemoveMovingObject (iMovingObject* mo)
{
  objects.Delete (mo);
}

//--------------------------------------------------------------------------

csBlobManager::csBlobManager (iBase *iParent) : scfImplementationType (this, iParent)
{
  object_reg = 0;
}

csBlobManager::~csBlobManager ()
{
  csHash<csMapTexture*,csString>::GlobalIterator it = mappedTextures.GetIterator ();
  while (it.HasNext ())
  {
    csMapTexture* mt = it.Next ();
    delete mt;
  }
}

bool csBlobManager::Initialize (iObjectRegistry* object_reg)
{
  csBlobManager::object_reg = object_reg;

  G3D = csQueryRegistry<iGraphics3D> (object_reg);
  G2D = G3D->GetDriver2D ();
  vc = csQueryRegistry<iVirtualClock> (object_reg);
  VFS = csQueryRegistry<iVFS> (object_reg);
  loader = csQueryRegistry<iLoader> (object_reg);
  engine = csQueryRegistry<iEngine> (object_reg);

  iFontServer* fntsvr = G2D->GetFontServer ();
  if (fntsvr)
  {
    fnt = fntsvr->LoadFont ("*large");
    int w;
    fnt->GetDimensions ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", w, fnt_height);
  }

  dirty_w = 1 + G2D->GetWidth ()/DIRTY_BLOCK_SIZE;
  dirty_h = 1 + G2D->GetHeight ()/DIRTY_BLOCK_SIZE;

  return true;
}

iBlobViewPort* csBlobManager::CreateBlobViewPort (int x1, int y1, int x2, int y2)
{
  csRef<csBlobViewPort> vp;
  vp.AttachNew (new csBlobViewPort (this, x1, y1, x2, y2));
  viewports.Push (vp);
  return vp;
}

iMovingObject* csBlobManager::FindMovingObject (int x, int y)
{
  size_t i;
  int layer;
  for (layer = NUM_LAYERS-1 ; layer >= 0 ; layer--)
  {
    i = movingobjects[layer].GetSize ();
    while (i > 0)
    {
      i--;
      iMovingObject* g = movingobjects[layer][i];
      if (g->IsClickable () && g->GetViewPort () == 0 && g->In (x, y))
        return g;
    }
  }
  i = viewports.GetSize ();
  while (i > 0)
  {
    i--;
    iMovingObject* mo = viewports[i]->FindMovingObject (x, y);
    if (mo) return mo;
  }
  return 0;
}

csBlobImage* csBlobManager::GetBlobImage (const char* name, iImageModifier* modifier)
{
  size_t i;
  for (i = 0 ; i < blob_images.GetSize () ; i++)
    if (blob_images[i]->GetName () == name && blob_images[i]->GetModifier () == modifier)
      return blob_images[i];
  return 0;
}

iBlobImage* csBlobManager::GetOrCreateBlobImage (const char* name, iImageModifier* modifier)
{
  // @@@ Optimize lookup of blob images by name.
  csBlobImage* bi = GetBlobImage (name, modifier);
  if (bi) return bi;

  bi = new csBlobImage (this, name, modifier, 0);
  blob_images.Push (bi);
  bi->DecRef ();
  return bi;
}

iBlobImage* csBlobManager::CreateBlobImage (const char* name,
      iImageModifier* modifier, const char* filename)
{
  csRef<csBlobImage> bi;
  bi.AttachNew (new csBlobImage (this, name, modifier, filename));
  blob_images.Push (bi);
  return bi;
}

void csBlobManager::RemoveBlobImage (iBlobImage* blobimage)
{
  blob_images.Delete (static_cast<csBlobImage*> (blobimage));
}

void csBlobManager::RemoveBlobImages ()
{
  blob_images.Empty ();
}

iBlobImage* csBlobManager::FindBlobImage (const char* name)
{
  size_t i;
  for (i = 0 ; i < blob_images.GetSize () ; i++)
    if (blob_images[i]->GetName () == name)
      return blob_images[i];
  return 0;
}

iBlob* csBlobManager::CreateBlob (const char* name, int layer, const char* filename,
    iImageModifier* modifier)
{
  csRef<csBlob> blob;
  blob.AttachNew (new csBlob (this, name, layer, filename, modifier));
  movingobjects[layer].Push (blob);
  return blob;
}

void csBlobManager::RemoveMovingObject (iMovingObject* blob)
{
  int l = blob->GetLayer ();
  blob->SetViewPort (0);
  movingobjects[l].Delete (blob);
}

void csBlobManager::RemoveMovingObjects ()
{
  int layer;
  for (layer = 0 ; layer < NUM_LAYERS ; layer++)
    movingobjects[layer].Empty ();
}

void csBlobManager::MovingObjectToFront (iMovingObject* b)
{
  b->IncRef ();
  movingobjects[b->GetLayer ()].Delete (b);
  movingobjects[b->GetLayer ()].Push (b);
  b->DecRef ();
}

bool csBlobManager::CheckCollision (iMovingObject* b1, iMovingObject* b2)
{
  int x1 = b1->GetRealX ();
  int y1 = b1->GetRealY ();
  int w1 = b1->GetWidth ();
  int h1 = b1->GetHeight ();

  int x2 = b2->GetRealX ();
  int y2 = b2->GetRealY ();
  int w2 = b2->GetWidth ();
  int h2 = b2->GetHeight ();

  if (x1+w1 <= x2) return false;
  if (x1 >= x2+w2) return false;
  if (y1+h1 <= y2) return false;
  if (y1 >= y2+h2) return false;
  return true;
}

iMovingObject* csBlobManager::CheckCollision (iMovingObject* b)
{
  int layer = b->GetLayer ();
  iBlobViewPort* vp = b->GetViewPort ();
  if (vp)
  {
    csBlobViewPort* viewport = static_cast<csBlobViewPort*> (vp);
    for (size_t i = 0 ; i < viewport->objects.GetSize () ; i++)
    {
      iMovingObject* o = viewport->objects.Get (i);
      if (o != b && o->GetLayer () == layer && CheckCollision (b, o))
        return o;
    }
  }
  else
  {
    for (size_t i = 0 ; i < movingobjects[layer].GetSize () ; i++)
    {
      iMovingObject* o = movingobjects[layer].Get (i);
      if (o != b && o->GetViewPort () == 0 && CheckCollision (b, o))
        return o;
    }
  }
  return 0;
}

iGeom* csBlobManager::CreateGeom (int layer, int w, int h)
{
  csRef<csGeom> geom;
  geom.AttachNew (new csGeom (this, layer, w, h));
  movingobjects[layer].Push (geom);
  return geom;
}

void csBlobManager::Update (csTicks current, csTicks elapsed)
{
  size_t i;
  //static csTicks prev = 0;
  //csTicks elapsed;
  //csTicks current = csGetTicks ();
  //if (prev == 0) elapsed = 0;
  //else elapsed = current-prev;
  //prev = current;
  //if (elapsed > 100) elapsed = 100;

  for (i = 0 ; i < viewports.GetSize () ; i++)
    viewports[i]->Update (current);

  int layer;
  for (layer = 0 ; layer < NUM_LAYERS ; layer++)
  {
    csRefArray<iMovingObject>& mo = movingobjects[layer];
    for (i = 0 ; i < mo.GetSize () ; i++)
      mo[i]->Update (elapsed);
  }
  RenderFrame ();
}

void csBlobManager::RenderFrame ()
{
  G3D->BeginDraw (CSDRAW_2DGRAPHICS | CSDRAW_CLEARSCREEN);
  int layer;
  size_t i;
  for (layer = 0 ; layer < NUM_LAYERS ; layer++)
  {
    csRefArray<iMovingObject>& mo = movingobjects[layer];
    for (i = 0 ; i < mo.GetSize () ; i++)
    {
      mo[i]->Draw ();
    }
  }
}

void csBlobManager::LoadTexture (const char* name, const char* filename)
{
  loader->LoadTexture (name, filename, CS_TEXTURE_NOMIPMAPS+CS_TEXTURE_NOFILTER,
      G3D->GetTextureManager (), true, true, false);
}

