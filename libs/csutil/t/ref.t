/*
    Copyright (C) 2012 by Frank Richter

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTAManagerILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "csutil/refcount.h"

/*
 * Test for correct csRef<>/csPtr<> constructor behaviour.
 */
class RefTest : public CppUnit::TestFixture
{
public:
  void setUp();
  void tearDown();

  void testAttachNew();
  void testNew();
  void testAssignCsPtr();

  CPPUNIT_TEST_SUITE(RefTest);
    CPPUNIT_TEST(testAttachNew);
    CPPUNIT_TEST(testNew);
    CPPUNIT_TEST(testAssignCsPtr);
  CPPUNIT_TEST_SUITE_END();
};

struct A : public csRefCount
{
};

struct B : public A
{
};

void RefTest::setUp()
{
}

void RefTest::tearDown()
{
}

void RefTest::testAttachNew()
{
  {
    csRef<A> a1;
    a1.AttachNew (new A);
    CPPUNIT_ASSERT_EQUAL (a1->GetRefCount(), 1);
  }
  {
    csRef<A> a2;
    a2.AttachNew (new B);
    CPPUNIT_ASSERT_EQUAL (a2->GetRefCount(), 1);
  }
}

void RefTest::testNew()
{
  {
    csRef<A> a1 (new A);
    CPPUNIT_ASSERT_EQUAL (a1->GetRefCount(), 2);
    a1->DecRef(); // Remove excess reference
  }
  {
    csRef<A> a2 (new B);
    CPPUNIT_ASSERT_EQUAL (a2->GetRefCount(), 2);
    a2->DecRef(); // Remove excess reference
  }
}

void RefTest::testAssignCsPtr()
{
  {
    csRef<A> a1;
    a1 = csPtr<A> (new A);
    CPPUNIT_ASSERT_EQUAL (a1->GetRefCount(), 1);
  }
  {
    csRef<A> a2;
    a2 = csPtr<A> (new B);
    CPPUNIT_ASSERT_EQUAL (a2->GetRefCount(), 1);
  }
  {
    csRef<B> b1;
    b1 = csPtr<B> (new B);
    CPPUNIT_ASSERT_EQUAL (b1->GetRefCount(), 1);
  }
  {
    csRef<B> b2;
    b2 = csPtr<B> (new B);
    CPPUNIT_ASSERT_EQUAL (b2->GetRefCount(), 1);
    csRef<A> a3;
    a3 = b2;
    CPPUNIT_ASSERT_EQUAL (b2->GetRefCount(), 2);
    CPPUNIT_ASSERT_EQUAL (a3->GetRefCount(), 2);
  }
  {
    csRef<A> a1;
    a1 = csPtr<A> (new A);
    CPPUNIT_ASSERT_EQUAL (a1->GetRefCount(), 1);
  }
  {
    csRef<A> a3;
    a3 = csPtr<A> (new A);
    CPPUNIT_ASSERT_EQUAL (a3->GetRefCount(), 1);
    csPtr<A> ap3 (a3);
    CPPUNIT_ASSERT_EQUAL (a3->GetRefCount(), 2);
    csRef<A> a4 (ap3);
    CPPUNIT_ASSERT_EQUAL (a3->GetRefCount(), 2);
    CPPUNIT_ASSERT_EQUAL (a4->GetRefCount(), 2);
  }
  {
    csRef<B> b3;
    b3 = csPtr<B> (new B);
    CPPUNIT_ASSERT_EQUAL (b3->GetRefCount(), 1);
    csPtr<A> ap4 (b3);
    CPPUNIT_ASSERT_EQUAL (b3->GetRefCount(), 2);
    csRef<A> a5 (ap4);
    CPPUNIT_ASSERT_EQUAL (b3->GetRefCount(), 2);
    CPPUNIT_ASSERT_EQUAL (a5->GetRefCount(), 2);
  }
}
