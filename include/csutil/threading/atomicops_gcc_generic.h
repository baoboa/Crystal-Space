/*
  Copyright (C) 2010 by Stefano Angeleri

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
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

#ifndef __CS_CSUTIL_ATOMICOPS_GCC_GENERIC_H__
#define __CS_CSUTIL_ATOMICOPS_GCC_GENERIC_H__

#ifndef DOXYGEN_RUN

namespace CS
{
namespace Threading
{
  class CS_CRYSTALSPACE_EXPORT AtomicOperationsGenericGCC
  {
    public:
    inline static int32 Set (int32* target, int32 value)
    {
      return __sync_lock_test_and_set (target, value);
    }

    inline static void* Set (void** target, void* value)
    {
      return __sync_lock_test_and_set (target, value);
    }

    inline static int32 CompareAndSet (int32* target, int32 value,
      int32 comparand)
    {
      return __sync_val_compare_and_swap (target, comparand, value);
    }

    inline static void* CompareAndSet (void** target, void* value,
      void* comparand)
    {
      return __sync_val_compare_and_swap (target, comparand, value);
    }

    inline static int32 Increment (int32* target, register int32 incr = 1)
    {
      return __sync_fetch_and_add (target, incr);
    }

    inline static int32 Decrement (int32* target)
    {
      return __sync_fetch_and_sub (target, 1);
    }
  };
}
}

#endif // DOXYGEN_RUN

#endif // __CS_CSUTIL_ATOMICOPS_GCC_GENERIC_H__
