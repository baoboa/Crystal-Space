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

#ifndef __CS_IVARIA_BLOBS_H__
#define __CS_IVARIA_BLOBS_H__

/**\file
 * Sequences
 */

#include "csutil/cscolor.h"
#include "csutil/scf_interface.h"
#include "igraphic/image.h"

struct iFont;

struct iBlobViewPort;

/**
 * An image modifier.
 * Implementations of this interface are responsible for modifying
 * a source image in some way and producing a new modified image as a result.
 */
struct iImageModifier : public virtual iBase
{
  SCF_INTERFACE (iImageModifier, 1, 0, 0);
  virtual const char* GetName () const = 0;
  virtual csRef<iImage> Modify (iImage* source) = 0;
};

/**
 * A blob image.
 * This is mostly useful for internal purposes.
 */
struct iBlobImage : public virtual iBase
{
  SCF_INTERFACE (iBlobImage, 1, 0, 0);
};

/**
 * A moving object.
 * Blobs and geoms can move and implement this interface.
 */
struct iMovingObject : public virtual iBase
{
  SCF_INTERFACE (iMovingObject, 1, 0, 0);

  /// The viewport on which this moving object moves.
  virtual void SetViewPort (iBlobViewPort* viewport) = 0;
  virtual iBlobViewPort* GetViewPort () = 0;

  /// Every moving object has a unique id.
  virtual uint32 GetID () const = 0;
  virtual const char* GetName () const = 0;

  /**
   * The layer of a moving object defines the priority for rendering.
   * Moving objects with a lower layer number will be rendered first.
   * Other objects are rendered on top of them. Within a layer the render
   * order is undefined.
   */
  virtual int GetLayer () const = 0;

  /**
   * Update the moving object based on time. This is typically not called
   * by the user.
   */
  virtual void Update (csTicks elapsed) = 0;
  /**
   * Draw the moving object. This is typically not called by the user.
   */
  virtual void Draw () = 0;

  /// Move the moving object.
  virtual void Move (int x, int y) = 0;
  /// Scale the moving object.
  virtual void Scale (int w, int h) = 0;
  /// Test if a given coordinate is in the moving object.
  virtual bool In (int x, int y) = 0;
  virtual int GetX () const = 0;
  virtual int GetY () const = 0;
  virtual int GetWidth () const = 0;
  virtual int GetHeight () const = 0;
  virtual int GetRealX () = 0;
  virtual int GetRealY () = 0;

  virtual void Hide () = 0;
  virtual void Show () = 0;
  virtual bool IsHidden () const = 0;

  /**
   * FindMovingObject will only find objects if they are clickable.
   */
  virtual void SetClickable (bool click) = 0;
  virtual bool IsClickable () const = 0;

  /**
   * Let the blob move along a specific line during the specified time.
   */
  virtual void SetLineMover (int x1, int y1, int x2, int y2, csTicks totaltime) = 0;

  /**
   * Let the blob move along a specific path. With SetPathMover() you set the
   * start location of the path. Use AddPathSegment() to add path segments then.
   */
  virtual void SetPathMover (int start_x, int start_y) = 0;
  /**
   * Add a path segment with a specific time for this segment.
   */
  virtual void AddPathSegment (int x, int y, csTicks segmenttime) = 0;
};

/**
 * A piece of geometry.
 * This is a moving object representing a number of drawing primitives.
 */
struct iGeom : public virtual iMovingObject
{
  SCF_INTERFACE (iGeom, 1, 0, 0);

  /// Add a primitive. The returned ID can be used to setup or remove the primitive.
  virtual uint32 Box (int x, int y, int w, int h, int color) = 0;
  virtual uint32 Line (int x1, int y1, int x2, int y2, int color) = 0;
  virtual uint32 Text (int x, int y, int fg, int bg, const char* text) = 0;
  virtual uint32 Blob (int x, int y, const char* name) = 0;

  virtual void Remove (uint32 id) = 0;

  virtual void ChangeColor (uint32 id, int color) = 0;
  virtual void ChangeText (uint32 id, const char* text) = 0;
  virtual void ChangeBlobImage (uint32 id, const char* name) = 0;
  virtual void ScaleGeom (uint32 id, int w, int h) = 0;
  virtual int GetGeomWidth (uint32 id) = 0;
  virtual int GetGeomHeight (uint32 id) = 0;
};

/**
 * A blob.
 * This is a moving object representing an image.
 */
struct iBlob : public virtual iMovingObject
{
  SCF_INTERFACE (iBlob, 1, 0, 0);

  /**
   * When a blob is created it automatically gets an image with the tag
   * '__main__'. Using AddImage() one can create additional images that are
   * layered on top of the first one.
   */
  virtual void AddImage (const char* tag, const char* name, size_t index = 100000,
      int offsetx = 0, int offsety = 0, int w = 0, int h = 0) = 0;
  /**
   * Remove all secondary images (all images with tag different from '__main__'.
   */
  virtual void RemoveSecondaryImages () = 0;

  /**
   * Set a modifier on this blob.
   */
  virtual void SetModifier (iImageModifier* modifier) = 0;
  virtual void ClearModifier () = 0;

  /// Set alpha to a specific value.
  virtual void SetAlpha (float alpha) = 0;
  /// Animate alpha over time.
  virtual void AnimateAlpha (float destalpha, csTicks ticks) = 0;

  /// Set a horizontal swap for one of the images.
  virtual void SetHorizontalSwap (bool sw, const char* tag) = 0;

  /**
   * Add an animation image to a specific animation.
   */
  virtual void AddAnimationImage (const char* animname, const char* image) = 0;
  /**
   * Play an animation. Basically this will replace the '__main__' image
   * of this blob with the images specified by the animation.
   * Only one animation can be active at the same time. If another animation
   * is already running it will be stopped.
   */
  virtual csTicks PlayAnimation (const char* animname, bool loop,
      bool remain = false, csTicks delay = 200) = 0;
  /**
   * Stop all animations.
   */
  virtual void StopAnimations () = 0;
};

/**
 * A blob viewport.
 * A viewport represents a collection of blobs that move together. Viewports
 * can scroll independently from each other and you can also set modifiers
 * on an entire viewport at once.
 */
struct iBlobViewPort : public virtual iBase
{
  SCF_INTERFACE (iBlobViewPort, 1, 0, 0);

  /**
   * Set the default scroll speed factor as used by ScrollRelative
   * and ScrollVisible. Default is 1.0f which is very fast. Bigger numbers
   * mean the scrolling will take longer.
   */
  virtual void SetScrollSpeed (float factor) = 0;
  virtual float GetScrollSpeed () const = 0;

  virtual bool Scroll (int x, int y) = 0;
  virtual int GetScrollX () const = 0;
  virtual int GetScrollY () const = 0;
  virtual csTicks ScrollRelative (int dx, int dy) = 0;
  virtual csTicks ScrollVisible (int x, int y, int margin) = 0;
  virtual void ClearModifier () = 0;
  virtual void SetModifier (iImageModifier* modifier, int layer = -1) = 0;
};

/**
 * The blob manager.
 */
struct iBlobManager : public virtual iBase
{
  SCF_INTERFACE (iBlobManager, 1, 1, 0);

  /**
   * Update all blobs and render them. This will automatically switch
   * the renderer to 2D mode.
   */
  virtual void Update (csTicks current, csTicks elapsed) = 0;

  /// Create a modifier that converts an image to grayscale.
  virtual csPtr<iImageModifier> GetGrayScaleImageModifier (
      const char* name) = 0;
  /// Create a modifier that modifies the color of the source image.
  virtual csPtr<iImageModifier> GetColorizedImageModifier (
      const char* name, const csColor4& mult,
      const csColor4& add) = 0;
  /// Create a modifier that blurs the source image.
  virtual csPtr<iImageModifier> GetBlurImageModifier (
      const char* name) = 0;
  /// Create a modifier that combines two other modifiers.
  virtual csPtr<iImageModifier> GetCombinedImageModifier (
      const char* name,
      iImageModifier* m1, iImageModifier* m2) = 0;

  /// Load a texture to be used for blobs.
  virtual void LoadTexture (const char* name, const char* filename) = 0;
  /**
   * Define a smaller texture that represents a part of a bigger texture.
   * When defining blobs or animations you can use small or big textures
   * automatically.
   */
  virtual void MapTexture (const char* bigtxt, const char* smalltxt,
      int x, int y, int w, int h) = 0;

  virtual iBlobViewPort* CreateBlobViewPort (int x1, int y1, int x2, int y2) = 0;

  /// Find a moving object. Only works on clickable objects.
  virtual iMovingObject* FindMovingObject (int x, int y) = 0;

  /// Create blob images. These functions are not usually called by users.
  virtual iBlobImage* CreateBlobImage (const char* name,
      iImageModifier* modifier = 0, const char* filename = 0) = 0;
  virtual void RemoveBlobImage (iBlobImage* blobimage) = 0;
  virtual void RemoveBlobImages () = 0;
  virtual iBlobImage* FindBlobImage (const char* name) = 0;

  /**
   * Create a new blob on a layer with a given image (name). If the image
   * cannot be found it will try to load it with the given 'filename'.
   * Note that currently only layers from 0 to 8 are supported.
   */
  virtual iBlob* CreateBlob (const char* name, int layer, const char* filename,
      iImageModifier* modifier) = 0;
  /**
   * Create a new geom object.
   */
  virtual iGeom* CreateGeom (int layer, int w, int h) = 0;
  virtual void RemoveMovingObject (iMovingObject* blob) = 0;
  virtual void RemoveMovingObjects () = 0;

  /**
   * Move a moving object so it appears in front on its given layer.
   */
  virtual void MovingObjectToFront (iMovingObject* b) = 0;

  /**
   * Check if two moving objects collide.
   */
  virtual bool CheckCollision (iMovingObject* b1, iMovingObject* b2) = 0;

  /**
   * Check if a moving object collides with another moving object on this
   * layer (and of the same viewport). Return the object.
   */
  virtual iMovingObject* CheckCollision (iMovingObject* b) = 0;
};

#endif // __CS_IVARIA_BLOBS_H__

