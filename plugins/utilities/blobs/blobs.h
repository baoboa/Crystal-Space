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

#ifndef __CS_BLOBS_H__
#define __CS_BLOBS_H__

#include "csutil/scf_implementation.h"
#include "csutil/util.h"
#include "csutil/weakref.h"
#include "csutil/refarr.h"
#include "csutil/parray.h"
#include "csutil/blockallocator.h"
#include "csutil/hash.h"
#include "csutil/eventnames.h"
#include "csutil/stringarray.h"
#include "ivaria/blobs.h"
#include "iutil/eventh.h"
#include "iutil/comp.h"
#include "iutil/vfs.h"
#include "iutil/virtclk.h"
#include "csgfx/imagemanipulate.h"
#include "ivideo/graph3d.h"
#include "ivideo/graph2d.h"
#include "ivideo/fontserv.h"
#include "imap/loader.h"
#include "iengine/engine.h"

#define NUM_LAYERS 9

class csBlob;
class csBlobManager;
class csBlobViewPort;

class csGrayScaleImageModifier : public scfImplementation1<
  csGrayScaleImageModifier,iImageModifier>
{
private:
  csString name;

public:
  csGrayScaleImageModifier (const char* name)
    : scfImplementationType (this), name (name) { }
  virtual ~csGrayScaleImageModifier () { }
  virtual const char* GetName () const
  {
    return (const char*)name;
  }
  virtual csRef<iImage> Modify (iImage* source)
  {
    return csImageManipulate::Gray (source);
  }
};

class csColorizedImageModifier : public scfImplementation1<
  csColorizedImageModifier,iImageModifier>
{
private:
  csString name;
  csColor4 mult;
  csColor4 add;

public:
  csColorizedImageModifier (const char* name,
      const csColor4& mult, const csColor4& add)
    : scfImplementationType (this), name (name),
      mult (mult), add (add) { }
  virtual ~csColorizedImageModifier () { }
  virtual const char* GetName () const
  {
    return (const char*)name;
  }
  virtual csRef<iImage> Modify (iImage* source)
  {
    return csImageManipulate::TransformColor (source, mult, add);
  }
};

class csBlurImageModifier : public scfImplementation1<
  csBlurImageModifier,iImageModifier>
{
private:
  csString name;

public:
  csBlurImageModifier (const char* name)
    : scfImplementationType (this), name (name) { }
  virtual ~csBlurImageModifier () { }
  virtual const char* GetName () const
  {
    return (const char*)name;
  }
  virtual csRef<iImage> Modify (iImage* source)
  {
    return csImageManipulate::Blur (source);
  }
};

class csCombinedImageModifier : public scfImplementation1<
  csCombinedImageModifier,iImageModifier>
{
private:
  csString name;
  csRefArray<iImageModifier> modifiers;

public:
  csCombinedImageModifier (const char* name)
    : scfImplementationType (this), name (name) { }
  virtual ~csCombinedImageModifier () { }
  virtual const char* GetName () const
  {
    return (const char*)name;
  }
  virtual csRef<iImage> Modify (iImage* source)
  {
    csRef<iImage> image = source;
    size_t i;
    for (i = 0 ; i < modifiers.GetSize () ; i++)
      image = modifiers[i]->Modify (image);
    return image;
  }
  void AddModifier (iImageModifier* mod)
  {
    modifiers.Push (mod);
  }
};

class csBlobImage : public scfImplementation1<csBlobImage, iBlobImage>
{
private:
  csBlobManager* mgr;
  csString name;
  csRef<iImageModifier> modifier;
  csString filename;
  int ox, oy, ow, oh;
  iTextureWrapper* txt;
  iTextureHandle* txthandle;
  int txt_w, txt_h;

private:
  csString ConstructTxtName (const char* name, iImageModifier* modifier)
  {
    csString n;
    if (modifier)
    {
      n = "__";
      n += modifier->GetName ();
      n += "__";
      n += name;
    }
    else
      n = name;
    return n;
  }
  void DoModifier ();
  void Setup ();

public:
  csBlobImage (csBlobManager* mgr, const char* name, iImageModifier* modifier,
      const char* filename);
  virtual ~csBlobImage () { }

  iTextureHandle* GetTextureHandle () const { return txthandle; }
  const csString& GetName () const { return name; }
  iImageModifier* GetModifier () const { return modifier; }

  void GetOffset (int& ox, int& oy) { ox = csBlobImage::ox; oy = csBlobImage::oy; }
  void GetDimensions (int& w, int& h)
  {
    if (ow == 0)
    {
      w = txt_w;
      h = txt_h;
    }
    else
    {
      w = ow;
      h = oh;
    }
  }
  bool HasModifier () const { return modifier != 0; }
};

class csBlobMover
{
public:
  virtual ~csBlobMover () { }
  virtual bool Update (csTicks t, iMovingObject* blob) = 0;
};

class csLineBlobMover : public csBlobMover
{
private:
  int x1, y1, x2, y2;
  float totaltime;
  float currenttime;

protected:
  // The time that was taken too much when this mover finishes.
  // This is used by csPathBlobMover to correct for the added time with
  // multiple segments.
  float time_already_taken;

public:
  csLineBlobMover () { currenttime = 0.0f; }
  csLineBlobMover (int x1, int y1, int x2, int y2, csTicks totaltime) :
    x1 (x1), y1 (y1), x2 (x2), y2 (y2), totaltime (float (totaltime))
  {
    currenttime = 0.0f;
  }
  virtual ~csLineBlobMover () { }
  void Setup (int x1, int y1, int x2, int y2, csTicks totaltime)
  {
    csLineBlobMover::x1 = x1;
    csLineBlobMover::y1 = y1;
    csLineBlobMover::x2 = x2;
    csLineBlobMover::y2 = y2;
    csLineBlobMover::totaltime = totaltime;
    currenttime = 0.0f;
  }
  virtual bool Update (csTicks t, iMovingObject* blob);
};

class csPathSegment
{
public:
  int x, y;
  csTicks segmenttime;

  csPathSegment () { segmenttime = 0; }
  csPathSegment (int x, int y, csTicks segmenttime)
    : x (x), y (y),segmenttime (segmenttime) { }
};

class csPathBlobMover : public csLineBlobMover
{
private:
  csArray<csPathSegment> segments;
  csPathSegment seg;
  int sx, sy;

public:
  csPathBlobMover (int sx, int sy) : csLineBlobMover (), sx (sx), sy (sy)
  {
  }
  virtual ~csPathBlobMover () { }
  void AddSegment (int x, int y, csTicks segmenttime)
  {
    segments.Push (csPathSegment (x, y, segmenttime));
  }
  virtual bool Update (csTicks t, iMovingObject* blob)
  {
    if (seg.segmenttime == 0)
    {
      if (segments.GetSize () > 0)
      {
	seg = segments[0];
	segments.DeleteIndex (0);
	Setup (sx, sy, seg.x, seg.y, seg.segmenttime);
      }
      else return true;
    }
    bool done = csLineBlobMover::Update (t, blob);
    if (done)
    {
      if (segments.GetSize () > 0)
      {
	int px = seg.x;
	int py = seg.y;
	seg = segments[0];
	segments.DeleteIndex (0);
	int segtime = int (seg.segmenttime) - int (time_already_taken);
	while (segtime <= 0)
	{
	  if (segments.GetSize () <= 0)
	    return true;
	  time_already_taken -= seg.segmenttime;
	  seg = segments[0];
	  segments.DeleteIndex (0);
	  segtime = int (seg.segmenttime) - int (time_already_taken);
	}
	Setup (px, py, seg.x, seg.y, csTicks (segtime));
      }
      else return true;
    }
    return false;
  }
};

class csBlobImageInstance
{
public:
  csString tag;
  csBlobImage* bi;
  bool swap;
  int offsetx, offsety;
  int w, h;
  csBlobImageInstance (const char* tag, csBlobImage* bi, bool swap) :
    tag (tag), bi (bi), swap (swap), offsetx (0), offsety (0), w (0), h (0) { }
};

class csBlobAnimation
{
public:
  csString name;
  csStringArray images;
  csBlobAnimation (const char* name) : name (name) { }
};

class csBlobViewPort : public scfImplementation1<csBlobViewPort, iBlobViewPort>
{
public:
  csBlobManager* mgr;
  int x1, y1, x2, y2;
  int scrollx, scrolly;
  float scrollspeed;
  csArray<iMovingObject*> objects;

  float start_scrollx;
  float start_scrolly;
  float end_scrollx;
  float end_scrolly;
  float scroll_start_time;
  float scroll_total_time;

  csTicks InitiateScroll (int newx, int newy);

public:
  csBlobViewPort (csBlobManager* mgr, int x1, int y1, int x2, int y2)
    : scfImplementationType (this), mgr (mgr), x1 (x1), y1 (y1), x2 (x2), y2 (y2)
  {
    scrollx = scrolly = 0;
    scroll_total_time = -1.0f;
    scrollspeed = 1.0f;
  }
  virtual ~csBlobViewPort () { }
  bool Update (csTicks current);
  virtual bool Scroll (int x, int y);
  virtual void SetScrollSpeed (float factor) { scrollspeed = factor; }
  virtual float GetScrollSpeed () const { return scrollspeed; }
  virtual int GetScrollX () const { return scrollx; }
  virtual int GetScrollY () const { return scrolly; }
  virtual csTicks ScrollRelative (int dx, int dy);
  virtual csTicks ScrollVisible (int x, int y, int margin);
  virtual void ClearModifier ();
  virtual void SetModifier (iImageModifier* modifier, int layer = -1);
  iMovingObject* FindMovingObject (int x, int y);
  void AddMovingObject (iMovingObject* mo);
  void RemoveMovingObject (iMovingObject* mo);
};

struct csGeomObject
{
  virtual ~csGeomObject ()  { }
  virtual void Draw (csBlobManager* mgr, int px, int py) = 0;
  virtual void ChangeColor (int color) = 0;
  virtual void ChangeText (const char* text) = 0;
  virtual void ChangeBlobImage (const char* name) = 0;
  virtual void ScaleGeom (int w, int h) = 0;
  virtual int GetGeomWidth () = 0;
  virtual int GetGeomHeight () = 0;
};

struct csGeomLine : public csGeomObject
{
  int x1, y1, x2, y2;
  int color;
  csGeomLine (int x1, int y1, int x2, int y2, int color)
    : x1 (x1), y1 (y1), x2 (x2), y2 (y2), color (color) { }
  virtual ~csGeomLine () { }
  virtual void Draw (csBlobManager* mgr, int px, int py);
  virtual void ChangeColor (int color) { csGeomLine::color = color; }
  virtual void ChangeText (const char*) { }
  virtual void ChangeBlobImage (const char*) { }
  virtual void ScaleGeom (int, int) { }
  virtual int GetGeomWidth () { return x2-x1+1; }
  virtual int GetGeomHeight () { return y2-y1+1; }
};

struct csGeomBox : public csGeomObject
{
  int x, y, w, h;
  int color;
  csGeomBox (int x, int y, int w, int h, int color)
    : x (x), y (y), w (w), h (h), color (color) { }
  virtual ~csGeomBox () { }
  virtual void Draw (csBlobManager* mgr, int px, int py);
  virtual void ChangeColor (int color) { csGeomBox::color = color; }
  virtual void ChangeText (const char*) { }
  virtual void ChangeBlobImage (const char*) { }
  virtual void ScaleGeom (int w, int h) { csGeomBox::w = w; csGeomBox::h = h; }
  virtual int GetGeomWidth () { return w; }
  virtual int GetGeomHeight () { return h; }
};

struct csGeomText : public csGeomObject
{
  int x, y;
  int fg, bg;
  csStringArray text;

  csGeomText (int x, int y, int fg, int bg, const char* text)
    : x (x), y (y), fg (fg), bg (bg)
  {
    ChangeText (text);
  }
  virtual ~csGeomText () { }
  virtual void Draw (csBlobManager* mgr, int px, int py);
  virtual void ChangeColor (int color) { fg = color; }
  virtual void ChangeText (const char* t);
  virtual void ChangeBlobImage (const char*) { }
  virtual void ScaleGeom (int, int) { }
  virtual int GetGeomWidth () { return 0; }
  virtual int GetGeomHeight () { return 0; }
};

struct csGeomBlob : public csGeomObject
{
  int x, y;
  csString name;
  csBlob* blob;

  csGeomBlob (csBlobManager* mgr, int x, int y, const char* name);
  virtual ~csGeomBlob ();
  virtual void Draw (csBlobManager*, int px, int py);
  virtual void ChangeColor (int) { }
  virtual void ChangeText (const char*) { }
  virtual void ChangeBlobImage (const char* name);
  virtual void ScaleGeom (int w, int h);
  virtual int GetGeomWidth ();
  virtual int GetGeomHeight ();
};

class csGeom : public scfImplementation1<csGeom, iGeom>
{
private:
  csBlobManager* mgr;
  int layer;
  csBlobViewPort* viewport;
  int x, y, w, h;
  bool hidden;
  bool clickable;
  csBlobMover* mover;
  uint32 gid;
  uint32 id;

  csPDelArray<csGeomObject> geometries;
  csHash<csGeomObject*,uint32> geometries_hash;
  bool GetRealPosInternal (int& px, int& py);

public:
  csGeom (csBlobManager* mgr, int layer, int w, int h);
  virtual ~csGeom ();
  virtual const char* GetName () const { return "<geom>"; }

  virtual int GetLayer () const { return layer; }

  virtual void Update (csTicks elapsed);
  virtual void Draw ();

  virtual void SetViewPort (iBlobViewPort* viewport);
  virtual iBlobViewPort* GetViewPort ()
  {
    return viewport;
  }
  virtual uint32 GetID () const { return id; }

  virtual void Move (int x, int y);
  virtual void Scale (int w, int h)
  {
    csGeom::w = w;
    csGeom::h = h;
  }
  virtual bool In (int x, int y);
  virtual int GetX () const { return x; }
  virtual int GetY () const { return y; }
  virtual int GetWidth () const { return w; }
  virtual int GetHeight () const { return h; }
  virtual int GetRealX ();
  virtual int GetRealY ();

  virtual void Hide () { hidden = true; }
  virtual void Show () { hidden = false; }
  virtual bool IsHidden () const { return hidden; }
  virtual void SetClickable (bool click) { clickable = click; }
  virtual bool IsClickable () const { return clickable; }

  virtual void SetLineMover (int x1, int y1, int x2, int y2, csTicks totaltime);
  virtual void SetPathMover (int start_x, int start_y);
  virtual void AddPathSegment (int x, int y, csTicks segmenttime);

  virtual uint32 Box (int x, int y, int w, int h, int color);
  virtual uint32 Line (int x1, int y1, int x2, int y2, int color);
  virtual uint32 Text (int x, int y, int fg, int bg, const char* text);
  virtual uint32 Blob (int x, int y, const char* name);
  virtual void Remove (uint32 id);

  virtual void ChangeColor (uint32 id, int color);
  virtual void ChangeText (uint32 id, const char* text);
  virtual void ChangeBlobImage (uint32 id, const char* name);
  virtual void ScaleGeom (uint32 id, int w, int h);
  virtual int GetGeomWidth (uint32 id);
  virtual int GetGeomHeight (uint32 id);
};

class csBlob : public scfImplementation1<csBlob, iBlob>
{
private:
  csBlobManager* mgr;
  csString name;
  csRef<iImageModifier> modifier;
  int layer;
  csBlobViewPort* viewport;
  int x, y, w, h;
  int orig_w, orig_h;
  uint32 id;
  bool hidden;
  bool clickable;
  csBlobMover* mover;
  csArray<csBlobImageInstance> bi_list;
  csHash<csBlobAnimation*,csString> animations;

  uint8 ualpha;
  float alpha;
  float srcalpha;
  float destalpha;
  float destalpha_factor;
  int destalpha_ticks;

  csBlobAnimation* curanim;
  bool curanim_loop;
  bool curanim_remain;
  int curanim_index;
  int curanim_timeleft;
  csTicks anim_delay;
  csString curanim_origimg;

  csBlobImage* FindOrCreateBlobImage (const char* name, const char* filename,
      iImageModifier* modifier);
  size_t FindTaggedIndex (const char* tag);

  bool GetRealPosInternal (int& px, int& py);

public:
  csBlob (csBlobManager* mgr, const char* name, int layer,
      const char* filename, iImageModifier* modifier);
  virtual ~csBlob ();

  virtual const char* GetName () const { return name; }
  virtual int GetLayer () const { return layer; }

  virtual void Update (csTicks elapsed);
  virtual void Draw ();

  virtual void SetViewPort (iBlobViewPort* viewport);
  virtual iBlobViewPort* GetViewPort ()
  {
    return viewport;
  }
  virtual uint32 GetID () const { return id; }

  virtual void Move (int x, int y);
  virtual void Scale (int w, int h)
  {
    csBlob::w = w;
    csBlob::h = h;
  }
  virtual bool In (int x, int y);
  virtual int GetX () const { return x; }
  virtual int GetY () const { return y; }
  virtual int GetWidth () const { return w; }
  virtual int GetHeight () const { return h; }
  virtual int GetRealX ();
  virtual int GetRealY ();

  virtual void Hide () { hidden = true; }
  virtual void Show () { hidden = false; }
  virtual bool IsHidden () const { return hidden; }
  virtual void SetClickable (bool click) { clickable = click; }
  virtual bool IsClickable () const { return clickable; }

  virtual void SetLineMover (int x1, int y1, int x2, int y2, csTicks totaltime);
  virtual void SetPathMover (int start_x, int start_y);
  virtual void AddPathSegment (int x, int y, csTicks segmenttime);

  virtual void RemoveSecondaryImages ();
  virtual void AddImage (const char* tag, const char* name,
      size_t index = 100000, int offsetx = 0, int offsety = 0, int w = 0, int h = 0);
  virtual void ClearModifier ();
  virtual void SetModifier (iImageModifier* modifier);

  virtual void AnimateAlpha (float destalpha, csTicks ticks);
  virtual void SetAlpha (float alpha)
  {
    if (alpha == csBlob::alpha) return;
    csBlob::alpha = alpha;
    int a = int (alpha * 255.0);
    if (a > 255) a = 255;
    ualpha = uint8 (a);
  }
  virtual void SetHorizontalSwap (bool sw, const char* tag);

  virtual void AddAnimationImage (const char* animname, const char* image);
  virtual csTicks PlayAnimation (const char* animname, bool loop,
      bool remain = false, csTicks delay = 200);
  virtual void StopAnimations ();
};

#define DIRTY_BLOCK_SIZE 128

class csMapTexture
{
public:
  csString big;
  int x, y, w, h;
  csMapTexture (const char* big, int x, int y, int w, int h) :
    big (big), x (x), y (y), w (w), h (h) { }
};

class csBlobManager :
  public scfImplementation2<csBlobManager, iBlobManager, iComponent>
{
public:
  iObjectRegistry *object_reg;
  csRef<iGraphics3D> G3D;
  csRef<iGraphics2D> G2D;
  csRef<iLoader> loader;
  csRef<iEngine> engine;
  csRef<iVFS> VFS;
  csRef<iVirtualClock> vc;
  csRef<iFont> fnt;
  int fnt_height;
  csHash<csMapTexture*,csString> mappedTextures;

  int dirty_w, dirty_h;

  csRefArray<iMovingObject> movingobjects[NUM_LAYERS];
  csRefArray<csBlobImage> blob_images;
  csRefArray<csBlobViewPort> viewports;

public:
  csBlobManager (iBase *iParent);
  virtual ~csBlobManager ();

  virtual bool Initialize (iObjectRegistry *object_reg);
  virtual void Update (csTicks current, csTicks elapsed);

  void RenderFrame ();

  virtual csPtr<iImageModifier> GetGrayScaleImageModifier (
      const char* name)
  {
    return csPtr<iImageModifier> (new csGrayScaleImageModifier (name));
  }
  virtual csPtr<iImageModifier> GetColorizedImageModifier (
      const char* name, const csColor4& mult,
      const csColor4& add)
  {
    return csPtr<iImageModifier> (new csColorizedImageModifier (name, mult, add));
  }
  virtual csPtr<iImageModifier> GetBlurImageModifier (
      const char* name)
  {
    return csPtr<iImageModifier> (new csBlurImageModifier (name));
  }
  virtual csPtr<iImageModifier> GetCombinedImageModifier (
      const char* name,
      iImageModifier* m1, iImageModifier* m2)
  {
    csCombinedImageModifier* mod = new csCombinedImageModifier (name);
    mod->AddModifier (m1);
    mod->AddModifier (m2);
    return csPtr<iImageModifier> (mod);
  }

  virtual void LoadTexture (const char* name, const char* filename);
  virtual void MapTexture (const char* big, const char* small,
      int x, int y, int w, int h)
  {
    mappedTextures.Put (small, new csMapTexture (big, x, y, w, h));
  }

  virtual iBlobViewPort* CreateBlobViewPort (int x1, int y1, int x2, int y2);
  virtual iMovingObject* FindMovingObject (int x, int y);

  iBlobImage* GetOrCreateBlobImage (const char* name,
      iImageModifier* modifier);
  csBlobImage* GetBlobImage (const char* name, iImageModifier* modifier);
  virtual iBlobImage* CreateBlobImage (const char* name,
      iImageModifier* modifier = 0, const char* filename = 0);
  virtual void RemoveBlobImage (iBlobImage* blobimage);
  virtual void RemoveBlobImages ();
  virtual iBlobImage* FindBlobImage (const char* name);

  virtual iBlob* CreateBlob (const char* name, int layer, const char* filename,
      iImageModifier* modifier);
  virtual iGeom* CreateGeom (int layer, int w, int h);
  virtual void RemoveMovingObject (iMovingObject* blob);
  virtual void RemoveMovingObjects ();
  virtual void MovingObjectToFront (iMovingObject* b);
  virtual bool CheckCollision (iMovingObject* b1, iMovingObject* b2);
  virtual iMovingObject* CheckCollision (iMovingObject* b);
};

#endif // __CS_BLOBS_H__

