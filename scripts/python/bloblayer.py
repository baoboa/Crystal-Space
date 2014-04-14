#!/usr/bin/env python

# Copyright (C) 2012 by Jorrit Tyberghein
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

import types, string, re, sys, os, traceback, array
from array import array
from math import sqrt

try:
    from cspace import *
except ImportError:
    print "WARNING: Failed to import module cspace"
    traceback.print_exc()
    sys.exit(1) # die!!

#####
# Note: we are assuming a global 'object_reg'
# which will be defined later
#####
def report(severity, msg):
    """Reporting routine"""
    csReport(object_reg, severity, "crystalspace.application.python", msg)

def log(msg):
    """Log a message"""
    report(CS_REPORTER_SEVERITY_NOTIFY, msg)

def fatal_error(msg="fatal_error"):
    """A Panic & die routine"""
    report(CS_REPORTER_SEVERITY_ERROR,msg)
    sys.exit(1)

#####################################################################

class Coordinate(object):
    """
    A representation of a 2 dimensional coordinate
    """
    def __init__(self,x=0,y=0):
        self.x = x
        self.y = y
    def __str__(self):
        return '('+str(self.x)+','+str(self.y)+')'
    def __repr__(self):
        return '('+str(self.x)+','+str(self.y)+')'
    def Interpolate(self,pos2,d):
        return Coordinate(self.x + (pos2.x-self.x)*d, self.y + (pos2.y-self.y)*d)
    def Distance(self,other):
        dx = self.x - other.x
        dy = self.y - other.y
        return sqrt(dx * dx + dy * dy)

    def Get(self):
        return self.x,self.y
    def Set(self,x,y):
        self.x = x
        self.y = y
    def Add(self,pos2):
        return Coordinate(self.x+pos2.x,self.y+pos2.y)
    def Sub(self,pos2):
        return Coordinate(self.x-pos2.x,self.y-pos2.y)
    def Scale(self,scale):
        return Coordinate(self.x*scale,self.y*scale)

#####################################################################

class MouseEventHandler(object):
    """
    If you want an event handler for a moving object then
    the easiest way is to subclass this.
    """
    def OnDown(self,num,pos):
        pass
    def OnUp(self,num,pos):
        pass
    def OnMove(self,pos):
        pass

#####################################################################

class KeyEventHandler(object):
    """ Keyboard event handler """
    def OnKeyDown(self,key):
        pass

#####################################################################

class MovingObject(object):
    """
    The superclass for all moving objects.
    """
    def __init__(self,layer):
        self.layer = layer
        self.mover = None
        self.pos = Coordinate()
        self.grabbing = False
        self.hidden = False
        self.clickable = True
        self.handler = None
        self.key_handler = None
        self.inside_algo = self.default_inside
        self.viewport = None
        self.w = 0
        self.h = 0
        # Only used by the viewport system.
        self.visible = True
        app.AddMovingObject(self,layer)

    @classmethod
    def default_inside(cls,movingobject,pos):
        """
        Default implementation to test if a position is
        inside a moving object.
        """
        x,y = pos.Get()
        p=movingobject.GetRealPos()
        if p is None: return False
        sx,sy=p.x,p.y
        sw,sh=movingobject.w,movingobject.h
        if x>=sx and x<=(sx+sw-1):
            if y>=sy and y<=(sy+sh-1):
                return True
        return False

    def SetViewPort(self,viewport):
        self.viewport = viewport

    def IsHidden(self):
        return self.hidden
    def SetClickable(self,clickable):
        self.clickable = clickable
    def IsClickable(self):
        return self.clickable
    def SetEventHandler(self,handler):
        self.handler = handler
    def GetEventHandler(self):
        return self.handler
    def SetKeyEventHandler(self,handler):
        self.key_handler = handler
    def GetKeyEventHandler(self):
        return self.key_handler

    def Move(self,pos):
        self.pos = pos
        self.x2 = pos.x+self.w
        self.y2 = pos.y+self.h
    def Scale(self,w,h):
        self.w = w
        self.h = h
        self.csgeom.Scale(w,h)
    def In(self,pos):
        return self.inside_algo(self,pos)

    # Use this to change the algorithm that is used to hit this blob.
    # This function expects a function that has two parameters:
    # the moving object and a coordinate.
    def SetHitAlgo(self,algo):
        self.inside_algo = algo

    def SetGrabbing(self,grab):
        self.grabbing = grab

#####################################################################

class ImageModifier(object):
    """ An image modifier """
    def __init__(self,name):
        self.name = name

class GrayScaleImageModifier(ImageModifier):
    """ A modifier that makes a grayscale image """
    def __init__(self,name):
        ImageModifier.__init__(self,name)
        self.modifier = app.blobs.GetGrayScaleImageModifier(name)

class ColorizedImageModifier(ImageModifier):
    """
    A modifier that makes a colorized image.
    'mult' and 'add' are 4-tuples (r,g,b,a) containing floats.
    """
    def __init__(self,name,mult,add):
        ImageModifier.__init__(self,name)
        m = csColor4(mult[0],mult[1],mult[2],mult[3])
        a = csColor4(add[0],add[1],add[2],add[3])
        self.modifier = app.blobs.GetColorizedImageModifier(name,m,a)

class BlurImageModifier(ImageModifier):
    """ A modifier that makes a blurred image """
    def __init__(self,name):
        ImageModifier.__init__(self,name)
        self.modifier = app.blobs.GetBlurImageModifier(name)

class CombinedImageModifier(ImageModifier):
    """
    A modifier that takes a list of modifiers and makes a combined
    modifier.
    """
    def __init__(self,name,modifiers):
        ImageModifier.__init__(self,name)
        self.modifiers = modifiers
        print name
        print modifiers
        print modifiers[0]
        print modifiers[1]
        self.modifier = app.blobs.GetCombinedImageModifier(name,modifiers[0].modifier,
                modifiers[1].modifier)

#####################################################################

class Geom(MovingObject):
    """ Subclass of MovingObject for a bunch of geometry """
    def __init__(self,w,h,layer):
        self.csgeom = app.blobs.CreateGeom(layer,w,h)
        self.id = self.csgeom.GetID()
        MovingObject.__init__(self,layer)
        self.w = w
        self.h = h

    def SetViewPort(self,viewport):
        MovingObject.SetViewPort(self,viewport)
        self.csgeom.SetViewPort(viewport.csviewport)

    def GetRealPos(self):
        x = self.csgeom.GetRealX()
        y = self.csgeom.GetRealY()
        return Coordinate(x,y)

    def SetLineMover(self,pos2,totaltime):
        self.csgeom.SetLineMover(self.pos.x,self.pos.y,pos2.x,pos2.y,totaltime)
    def SetPathMover(self,pos):
        self.csgeom.SetPathMover(pos.x,pos.y)
    def AddPathSegment(self,pos,time):
        self.csgeom.AddPathSegment(pos.x,pos.y,time)

    def Box(self,pos1,w,h,color):
        return self.csgeom.Box(int(pos1.x),int(pos1.y),int(w),int(h),color)

    def Line(self,pos1,pos2,color):
        return self.csgeom.Line(int(pos1.x),int(pos1.y),int(pos2.x),int(pos2.y),color)

    def Text(self,pos,fg,bg,text):
        return self.csgeom.Text(int(pos.x),int(pos.y),fg,bg,str(text))

    def Blob(self,pos,name):
        return self.csgeom.Blob(int(pos.x),int(pos.y),str(name))

    def ChangeColor(self,id,color):
        self.csgeom.ChangeColor(id,color)
    def ChangeText(self,id,text):
        self.csgeom.ChangeText(id,text)

    def Remove(self,b):
        self.csgeom.Remove(b)

    def Hide(self):
        self.hidden = True
        self.csgeom.Hide()
    def Show(self):
        self.hidden = False
        self.csgeom.Show()
    def SetClickable(self,clickable):
        self.csgeom.SetClickable(clickable)
    def IsClickable(self):
        return self.csgeom.IsClickable()

    def Move(self,pos):
        self.pos = pos
        self.csgeom.Move(pos.x,pos.y)
    def Scale(self,w,h):
        self.w = w
        self.h = h
        self.x2 = self.pos.x+w
        self.y2 = self.pos.y+h
        self.csgeom.Scale(w,h)
    def In(self,pos):
        return self.inside_algo(self,pos)

#####################################################################

class Blob(MovingObject):
    """ Subclass of MovingObject for a blob """
    def __init__(self,name,layer,filename=None,modifier=None):
        self.name = name
        if name is None:
            print 'ERROR: blob with None'
            a = []
            print a[3]
        if modifier is not None:
            m = modifier.modifier
        else:
            m = None
        self.csblob = app.blobs.CreateBlob(str(name),layer,str(filename),m)
        self.id = self.csblob.GetID()
        self.w = self.csblob.GetWidth()
        self.h = self.csblob.GetHeight()
        self.orig_w = self.w
        self.orig_h = self.h
        self.anim_delay = 200
        MovingObject.__init__(self,layer)

    def __hash__(self):
        return hash(self.csblob.GetID())

    def __eq__(self,other):
        if other is None:
            return False
        if self is other:
            return True
        try:
            othercsblob = other.csblob
        except AttributeError:
            othercsblob = None
        if self.csblob is None and othercsblob is None:
            return True
        if self.csblob is None or othercsblob is None:
            return False
        return self.csblob.GetID () == othercsblob.GetID ()

    def GetRealPos(self):
        x = self.csblob.GetRealX()
        y = self.csblob.GetRealY()
        return Coordinate(x,y)

    def SetLineMover(self,pos2,totaltime):
        self.csblob.SetLineMover(self.pos.x,self.pos.y,pos2.x,pos2.y,totaltime)
    def SetPathMover(self,pos):
        self.csblob.SetPathMover(pos.x,pos.y)
    def AddPathSegment(self,pos,time):
        self.csblob.AddPathSegment(pos.x,pos.y,time)

    def SetViewPort(self,viewport):
        MovingObject.SetViewPort(self,viewport)
        self.csblob.SetViewPort(viewport.csviewport)

    def SetName(self,name):
        self.name = name

    # Remove all images except for __main__.
    def RemoveSecondaryImages(self):
        self.csblob.RemoveSecondaryImages()

    def AddImage(self,tag,name,index=100000):
        self.csblob.AddImage(str(tag),str(name),index)

    def ClearModifier(self):
        if self.csblob:
            self.csblob.ClearModifier()

    def SetModifier(self,modifier):
        self.ClearModifier()
        if self.csblob:
            self.csblob.SetModifier(modifier.modifier)

    def AnimateAlpha(self,destalpha,ticks):
        self.csblob.AnimateAlpha(destalpha,ticks)

    def SetAlpha(self,alpha):
        self.csblob.SetAlpha(alpha)
    def SetHorizontalSwap(self,sw,tag='__main__'):
        self.csblob.SetHorizontalSwap(sw,tag)

    def AddAnimationImage(self,animname,image):
        self.csblob.AddAnimationImage(str(animname),str(image))
    def PlayAnimation(self,animname,loop,remain = False,delay=200):
        self.anim_delay = delay
        return self.csblob.PlayAnimation(animname,loop,remain,delay)
    def StopAnimations(self):
        self.csblob.StopAnimations()

    def Hide(self):
        self.hidden = True
        self.csblob.Hide()
    def Show(self):
        self.hidden = False
        self.csblob.Show()
    def SetClickable(self,clickable):
        self.csblob.SetClickable(clickable)
    def IsClickable(self):
        return self.csblob.IsClickable()

    def Move(self,pos):
        self.pos = pos
        self.csblob.Move(int(pos.x),int(pos.y))
    def Scale(self,w,h):
        self.w = w
        self.h = h
        self.csblob.Scale(w,h)
    def In(self,pos):
        return self.csblob.In(int(pos.x),int(pos.y))
        #return self.inside_algo(self,pos)

#####################################################################

class ViewPort(object):
    """
    A viewport. This defines an area on screen where moving objects
    are visible. Viewports can be scrolled and also do (soft) clipping
    of the objects on the viewport. Objects on a viewport have
    a position relative to the top-left coordinate of the viewport.
    """
    def __init__(self,x1,y1,x2,y2):
        app.cs_viewports.append(self)
        self.csviewport = app.blobs.CreateBlobViewPort(x1,y1,x2,y2)

    def Update(self,curtime):
        pass

    # Returns the amount of time needed to get to the location.
    def ScrollRelative(self,dx,dy):
        return self.csviewport.ScrollRelative(int(dx),int(dy))
    # Scroll immediatelly.
    def Scroll(self,sp):
        self.csviewport.Scroll(int(sp.x),int(sp.y))
    # Returns the amount of time needed to get to the location.
    def ScrollVisible(self,sp,margin):
        return self.csviewport.ScrollVisible(int(sp.x),int(sp.y),margin)

    def ClearModifier(self):
        self.csviewport.ClearModifier()
    def SetModifier(self,mod,layer=-1):
        self.csviewport.SetModifier(mod.modifier,layer)
    def GetScrollX(self):
        return self.csviewport.GetScrollX()
    def GetScrollY(self):
        return self.csviewport.GetScrollY()

#####################################################################

class Sound(object):
    """ A sound object """
    def __init__(self,filename):
        soundbuf = app.vfs.ReadFile(str(filename))
        if soundbuf == None:
            print 'Error reading file ', filename, '!'
            sys.exit(1)
        sounddata = app.sndloader.LoadSound(soundbuf)
        # @@@ Error check
        self.stream = app.sndrenderer.CreateStream(sounddata,CS_SND3D_DISABLE)
        # @@@ Error check
        self.source = app.sndrenderer.CreateSource(self.stream)
        # @@@ Error check
        self.source.SetVolume(1.0)
    def SetVolume(self,f):
        self.source.SetVolume(f)
    def SetLoop(self,l):
        if l:
            loop = CS_SNDSYS_STREAM_LOOP
        else:
            loop = CS_SNDSYS_STREAM_DONTLOOP
        self.stream.SetLoopState(loop)
    def Play(self):
        self.stream.ResetPosition()
        self.stream.Unpause()

#####################################################################

class CrystalSpace(object):
    """ Application """
    LAYER_UI = 7
    KEY_DOWN = CSKEY_DOWN
    KEY_UP = CSKEY_UP
    KEY_LEFT = CSKEY_LEFT
    KEY_RIGHT = CSKEY_RIGHT
    KEY_ENTER = 10
    KEY_BS = 8
    KEY_TAB = ord('\t')

    def Init(self):
        self.current_time = 0
        self.framecounter_time = 0
        self.framecounter = 0
        self.prev_time = 0

        self.vc = object_reg.Get(iVirtualClock)
        self.engine = object_reg.Get(iEngine)
        self.g3d = object_reg.Get(iGraphics3D)
        self.loader = object_reg.Get(iLoader)
        self.keybd = object_reg.Get(iKeyboardDriver)
        self.vfs = object_reg.Get(iVFS)
        self.sndloader = object_reg.Get(iSndSysLoader)
        self.sndrenderer = object_reg.Get(iSndSysRenderer)
        self.sndmgr = object_reg.Get(iSndSysManager)
        self.blobs = object_reg.Get(iBlobManager)

        if self.vc is None or self.engine is None or self.g3d is None or self.keybd is None or self.loader is None:
            fatal_error("Error: in object registry query")

        #plugmgr = object_reg.Get(iPluginManager)
        #self.bl = plugmgr.LoadPlugin('cel.behaviourlayer.python',iCelBlLayer)

        if not csInitializer.OpenApplication(object_reg):
            fatal_error("Could not open the application!")

        self.view=csView(self.engine,self.g3d)
        self.g2d = self.g3d.GetDriver2D()
        self.view.SetRectangle(0, 0, self.g2d.GetWidth(), self.g2d.GetHeight ())

        self.black = self.g2d.FindRGB(0,0,0)
        self.white = self.g2d.FindRGB(255,255,255)
        self.verydark_gray = self.g2d.FindRGB(40,40,40)
        self.dark_gray = self.g2d.FindRGB(100,100,100)
        self.gray = self.g2d.FindRGB(180,180,180)
        self.light_gray = self.g2d.FindRGB(220,220,220)
        self.transparent = -1

        self.red = self.g2d.FindRGB(200,0,0)
        self.green = self.g2d.FindRGB(0,200,0)
        self.blue = self.g2d.FindRGB(0,0,200)
        self.yellow = self.g2d.FindRGB(200,200,0)
        self.cyan = self.g2d.FindRGB(0,200,200)
        self.purple = self.g2d.FindRGB(200,0,200)

        self.dark_red = self.g2d.FindRGB(100,0,0)
        self.dark_green = self.g2d.FindRGB(0,100,0)
        self.dark_blue = self.g2d.FindRGB(0,0,100)
        self.dark_yellow = self.g2d.FindRGB(100,100,0)
        self.dark_cyan = self.g2d.FindRGB(0,100,100)
        self.dark_purple = self.g2d.FindRGB(100,0,100)

        self.light_red = self.g2d.FindRGB(255,0,0)
        self.light_green = self.g2d.FindRGB(0,255,0)
        self.light_blue = self.g2d.FindRGB(0,0,255)
        self.light_yellow = self.g2d.FindRGB(255,255,0)
        self.light_cyan = self.g2d.FindRGB(0,255,255)
        self.light_purple = self.g2d.FindRGB(255,0,255)

        self.font = self.g2d.GetFontServer().LoadFont('*large')
        fd = self.TextDimensions('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789')
        self.font_avg_width = fd[0]/62
        self.font_height = fd[1]
        self.font_descent = fd[2]
        print self.font_height, self.font_descent

        self.cs_movingobjects_byid = {}
        self.cs_viewports = []
        self.cs_blob_grab = None
        self.key_focus = None
        self.modal = None
        self.timers = []

        self.listener = self.sndrenderer.GetListener()

        self.Setup()

    def Mount(self,path,realpath):
        self.vfs.Mount(path,realpath.replace('/','$/'))

    def MakeRealPath(self,virtualpath):
        return virtualpath

    def GetWidth(self):
        return self.g2d.GetWidth()
    def GetHeight(self):
        return self.g2d.GetHeight()

    def Load(self,filename):
        result = None
        rc = self.loader.Load(filename,result)
        if not rc:
            fatal_error("Couldn't load file '"+filename+"'!")
        return rc

    def LoadTexture(self,name,filename):
        self.blobs.LoadTexture(str(name),str(filename))

    # Set some moving object as modal. It will be the only
    # moving object that gets events.
    def SetModal(self,mo):
        self.modal = mo
    def ClearModal(self):
        self.modal = None

    # Make a mapping from a small texture to some part in a bigger texture.
    def MapTexture(self,big,small,x,y,w,h):
        self.blobs.MapTexture(str(big),str(small),x,y,w,h)

    def Setup(self):
        pass

    def OnBlobDown(self,b,num,pos):
        return False
    def OnBlobUp(self,b,num,pos):
        return False
    def OnBlobMove(self,b,pos):
        return False

    def OnKeyDown(self,key):
        if self.key_focus != None:
            h = self.key_focus.GetKeyEventHandler()
            if h != None:
                h.OnKeyDown(key)

    def OnMouseDownDefault(self,num,pos,b):
        if b <> None and ((not self.modal) or self.modal == b):
            if b.GetEventHandler():
                b.GetEventHandler().OnDown(num,pos)
            else:
                self.OnBlobDown(b,num,pos)
            if b.grabbing:
                self.cs_blob_grab = b
                self.cs_grab_offset = Coordinate.Sub(pos,b.pos)
            return True
        elif self.modal:
            self.RemoveMovingObject(self.modal)
            self.modal = None
            return True
        return False
    def OnMouseDown(self,num,pos):
        if self.cs_blob_grab != None:
            return False
        b = self.FindMovingObject(pos)
        return self.OnMouseDownDefault(num,pos,b)

    def OnMouseMove(self,pos):
        b = self.FindMovingObject(pos)
        if self.cs_blob_grab == None:
            if b <> None and ((not self.modal) or self.modal == b):
                if b.GetEventHandler():
                    b.GetEventHandler().OnMove(pos)
                else:
                    self.OnBlobMove(b,pos)
            return
        b = self.cs_blob_grab
        if ((not self.modal) or self.modal == b):
            if b.GetEventHandler():
                b.GetEventHandler().OnMove(pos)
            else:
                self.OnBlobMove(b,pos)
            b.Move(Coordinate.Sub(pos,self.cs_grab_offset))

    def OnMouseUp(self,num,pos):
        if self.cs_blob_grab == None:
            b = self.FindMovingObject(pos)
            if b <> None and ((not self.modal) or self.modal == b):
                if b.GetEventHandler():
                    b.GetEventHandler().OnUp(num,pos)
                else:
                    self.OnBlobUp(b,num,pos)
        else:
            b = self.cs_blob_grab
            if ((not self.modal) or self.modal == b):
                if b.GetEventHandler():
                    b.GetEventHandler().OnUp(num,pos)
                else:
                    self.OnBlobUp(b,num,pos)
                self.cs_blob_grab = None

    def FindObjectById(self,id):
        return self.cs_movingobjects_byid[id]

    def FindMovingObject(self,pos):
        mo = self.blobs.FindMovingObject(pos.x,pos.y)
        if mo == None:
            return None
        id = mo.GetID()
        if id in self.cs_movingobjects_byid:
            return self.cs_movingobjects_byid[id]
        else:
            print id.__class__
            print len(self.cs_movingobjects_byid)
            print 'Weird, cannot find object with id', id, 'and name',mo.GetName(),'w',mo.GetWidth(),'h',mo.GetHeight(),'x',mo.GetX(),'y',mo.GetY(), '!'
            return None

    def AddMovingObject(self,b,layer):
        b.layer = layer
        self.cs_movingobjects_byid[b.id] = b
    def RemoveMovingObject(self,b):
        if self.modal == b:
            self.modal = None
        del self.cs_movingobjects_byid[b.id]
        if isinstance(b,Geom):
            self.blobs.RemoveMovingObject(b.csgeom)
            b.csgeom = None
        else:
            self.blobs.RemoveMovingObject(b.csblob)
            b.csblob = None

    def BlobToFront(self,b):
        self.blobs.MovingObjectToFront(b.csblob)

    def StartTimer(self,ticks,fun,dbg):
        #if len(self.timers) > 1:
            #output = '#t('+str(len(self.timers)+1)+')'
            #for (_,t,d) in self.timers:
                #output = output+' ('+str(t)+','+d+')'
            #output = output+' NEW:('+str(ticks)+','+dbg+')'
            #print output
        time = ticks + self.current_time
        if len(self.timers) == 0:
            self.timers.append((fun,time,dbg))
        else:
            done = False
            for i in xrange(0,len(self.timers)):
                if time < self.timers[i][1]:
                    self.timers.insert(i,(fun,time,dbg))
                    done = True
                    break
            if not done:
                self.timers.append((fun,time,dbg))

    def SetupFrame(self):
        #self.elapsed_time = self.vc.GetElapsedTicks()
        #self.current_time = self.vc.GetCurrentTicks()
        self.current_time = csGetTicks()
        if self.prev_time == 0:
            self.elapsed_time = 0
        else:
            self.elapsed_time = self.current_time - self.prev_time
        self.prev_time = self.current_time
        if self.elapsed_time > 100:
            self.elapsed_time = 100

        self.blobs.Update(self.current_time,self.elapsed_time)

        # Check timers.
        while len(self.timers) > 0 and self.current_time >= self.timers[0][1]:
            t = self.timers[0][0]
            del self.timers[0]
            t()

        # Tell 3D driver we're going to display 3D things.
        #if not self.g3d.BeginDraw(self.engine.GetBeginDrawFlags() | CSDRAW_2DGRAPHICS | CSDRAW_CLEARSCREEN):
        #fatal_error()
        self.Frame()

    def FinishFrame(self):
        self.g3d.FinishDraw()
        self.g3d.Print(None)

    def Frame(self):
        pass

    def GetElapsedTicks(self):
        return self.elapsed_time
    def Color(self,r,g,b):
        return self.g2d.FindRGB(int(r),int(g),int(b))
    def Line(self,pos1,pos2,color):
        self.g2d.DrawLine(float(pos1.x),float(pos1.y),float(pos2.x),float(pos2.y),color)
    def Box(self,pos1,pos2,color):
        self.g2d.DrawBox(int(pos1.x),int(pos1.y),int(pos2.x-pos1.x)+1,int(pos2.y-pos1.y)+1,color)
    def Pixel(self,pos,color):
        self.g2d.DrawPixel(int(pos.x),int(pos.y),color)
    def Write(self,pos,fg,bg,text):
        self.g2d.Write(self.font,int(pos.x),int(pos.y),fg,bg,text)
    def TextDimensions(self,text):
        return self.font.GetDimensions(text)

#####################################################################

def EventHandler(ev):
    """ EventHandler """
    if ev.Name == KeyboardDown:
        key = csKeyEventHelper.GetCookedCode(ev)
        if key == CSKEY_ESC:
            q = object_reg.Get(iEventQueue)
            if q:
                q.GetEventOutlet().Broadcast(csevQuit(object_reg))
        else:
            #mod = csKeyEventHelper.GetModifiers(ev)
            app.OnKeyDown(key)
        return 1
    elif ev.Name == Frame:
        app.SetupFrame()
        app.FinishFrame()
        return 1
    elif ev.Name == MouseDown:
        num = csMouseEventHelper.GetButton(ev)
        x = csMouseEventHelper.GetX(ev)
        y = csMouseEventHelper.GetY(ev)
        app.OnMouseDown(num,Coordinate(x,y))
        return 0
    elif ev.Name == MouseUp:
        num = csMouseEventHelper.GetButton(ev)
        x = csMouseEventHelper.GetX(ev)
        y = csMouseEventHelper.GetY(ev)
        app.OnMouseUp(num,Coordinate(x,y))
        return 0
    elif ev.Name == MouseMove:
        x = csMouseEventHelper.GetX(ev)
        y = csMouseEventHelper.GetY(ev)
        app.OnMouseMove(Coordinate(x,y))
        return 0
    return 0

#####################################################################

def StartCrystalSpace(application,config):
    """ # startup code """
    # @@@ put in 'app' instead of global!!!
    global object_reg
    global KeyboardDown
    global MouseDown
    global MouseUp
    global MouseMove
    global Frame
    global app
    app = application

    object_reg = csInitializer.CreateEnvironment(sys.argv)
    if object_reg is None:
        fatal_error("Couldn't create enviroment!")

    if csCommandLineHelper.CheckHelp(object_reg):
        csCommandLineHelper.Help(object_reg)
        sys.exit(0)

    if not csInitializer.SetupConfigManager(object_reg,config):
        fatal_error("Couldn't init app!")

    plugin_requests = [
        CS_REQUEST_VFS, CS_REQUEST_OPENGL3D, CS_REQUEST_ENGINE,
        CS_REQUEST_FONTSERVER, CS_REQUEST_IMAGELOADER, CS_REQUEST_LEVELLOADER,
        CS_REQUEST_PLUGIN('crystalspace.utilities.blobs',iBlobManager),
        CS_REQUEST_PLUGIN('crystalspace.sndsys.element.loader',iSndSysLoader),
        CS_REQUEST_PLUGIN('crystalspace.sndsys.renderer.software',iSndSysRenderer),
    ]
    if not csInitializer.RequestPlugins(object_reg, plugin_requests):
        fatal_error("Plugin requests failed!")

    # Get some often used event IDs
    KeyboardDown = csevKeyboardDown(object_reg)
    MouseDown = csevMouseDown(object_reg,0)
    MouseUp = csevMouseUp(object_reg,0)
    MouseMove = csevMouseMove(object_reg,0)
    Frame = csevFrame(object_reg)

    # setup the event handler:
    # note: we need not even make EventHandler() a global fn
    # python would accept it as a member fn of CrystalSpace
    eventids = [KeyboardDown, MouseDown, MouseUp, MouseMove, Frame,
                                    CS_EVENTLIST_END]
    if not csInitializer.SetupEventHandler(object_reg, EventHandler,eventids):
        fatal_error("Could not initialize event handler!")

    app.Init()    # turn on the app
    # this also now calls OpenApplication

    csDefaultRunLoop(object_reg)

    app=None        # need to do this or you get 'unreleased instances' warning
    # See! CsPython manages the smart pointers correctly

def StopCrystalSpace():
    global object_reg
    csInitializer.DestroyApplication (object_reg)     # bye bye
    object_reg=None # just to be complete (not really needed)

