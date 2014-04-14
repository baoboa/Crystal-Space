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

# Note: flare_01_0.png was fetched from:
#    http://opengameart.org/content/flare-effect
#    License: CC-BY 3.0

import bloblayer as cs
from bloblayer import Blob, Coordinate
import random

LAYER_BALLS = 0
LAYER_EXPLOSION = 1
LAYER_LOGO = 2

class Ball(object):
    """
    A moving ball that collides on impact with other balls
    """
    def __init__(self,app,pos):
        self.app = app
        self.blob = Blob('explo',LAYER_BALLS)
        self.blob.ball = self
        self.w = self.blob.csblob.GetWidth()
        self.h = self.blob.csblob.GetHeight()
        self.imaginarypos = Coordinate(pos.x-self.w/2, pos.y-self.h/2)
        self.blob.Move(self.imaginarypos)
        self.pick_random_pos()

    def destroy(self):
        self.app.RemoveMovingObject(self.blob)
        self.blob = None

    def pick_random_pos(self):
        x = random.randint(0,self.app.width)
        y = random.randint(0,self.app.height)
        self.desired = Coordinate(x,y)
        self.nexttime = random.random() * 1.0

    def __hash__(self):
        return hash(self.blob)

    def __eq__(self,other):
        return self.blob == other.blob

    def update(self,elapsed):
        self.nexttime -= elapsed
        if self.nexttime <= 0.0:
            self.pick_random_pos()

        distance = self.imaginarypos.Distance(self.desired)
        if distance > 0.00001:
            self.imaginarypos = self.imaginarypos.Interpolate(self.desired,900.0*elapsed/distance)

        distance = self.imaginarypos.Distance(self.blob.pos)
        if distance > 0.00001:
            self.blob.Move(self.blob.pos.Interpolate(self.imaginarypos,400.0*elapsed/distance))

        other = self.app.blobs.CheckCollision(self.blob.csblob)
        if other is not None:
            print 'Bam!'
            otherball = self.app.FindObjectById(other.GetID()).ball
            self.app.tocollide.add(otherball)
            self.app.tocollide.add(self)

class BlobExample(cs.CrystalSpace):
    """
    The main application class
    """
    def Setup(self):
        self.width = self.GetWidth()
        self.height = self.GetHeight()
        self.viewport = cs.ViewPort(0,0,self.width,self.height)

        self.LoadTexture('cslogo','/lib/stdtex/cslogo2.png')
        self.LoadTexture('explo','/lib/stdtex/explo.png')
        self.LoadTexture('flare','/lib/stdtex/flare_01_0.png')

        self.logo = Blob('cslogo',LAYER_LOGO)
        self.logo.Move(Coordinate(100,100))

        self.balls = set()
        self.tocollide = set()
        self.create_ball(Coordinate(100,100))

    def OnMouseDown(self,num,pos):
        if super(BlobExample,self).OnMouseDown(num,pos):
            return True
        self.create_ball(pos)
        return True

    def create_ball(self,pos):
        self.balls.add(Ball(self,pos))

    def create_balls(self,pos):
        dist = 200
        self.balls.add(Ball(self,pos.Add(Coordinate(-dist,-dist))))
        self.balls.add(Ball(self,pos.Add(Coordinate(-dist,dist))))
        self.balls.add(Ball(self,pos.Add(Coordinate(dist,-dist))))
        self.balls.add(Ball(self,pos.Add(Coordinate(dist,dist))))

    def start_flare(self,pos):
        flare = Blob('flare',LAYER_EXPLOSION)
        flare.Scale(300,300)
        w = flare.csblob.GetWidth()
        h = flare.csblob.GetHeight()
        flare.Move(Coordinate(pos.x-w/2, pos.y-h/2))
        flare.SetAlpha(0.0)
        flare.AnimateAlpha(1.0,1000)
        self.StartTimer(1050, lambda: self.remove_flare(flare),'flare')

    def remove_flare(self,flare):
        self.RemoveMovingObject(flare)

    def Frame(self):
        elapsed = self.vc.GetElapsedSeconds()
        for ball in self.balls:
            ball.update(elapsed)
        for ball in self.tocollide:
            pos = ball.blob.pos
            pos.x += ball.w
            pos.y += ball.h
            self.balls.remove(ball)
            ball.destroy()
            if random.randint(0,3) == 1:
                self.create_balls(pos)
            else:
                self.start_flare(pos)
        self.tocollide = set()

cs.StartCrystalSpace(BlobExample(),'/config/blobexample.cfg')

# This garbage collect is needed to enforce that the application instance
# above is completely gone before we start to destroy Crystal Space.
import gc
gc.collect()

cs.StopCrystalSpace()

