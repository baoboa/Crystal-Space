/*
  Copyright (C) 2012 by Frank Richter

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"

#include "iutil/verbositymanager.h"

#include "csutil/cfgacc.h"
#include "csutil/objreg.h"
#include "csutil/scf.h"

#include "docpreproc.h"
#include "docwrap.h"

CS_PLUGIN_NAMESPACE_BEGIN(DocPreproc)
{
  SCF_IMPLEMENT_FACTORY(DocumentPreprocessor)

  DocumentPreprocessor::DocumentPreprocessor (iBase* scfParent)
    : scfImplementationType (this, scfParent), do_verbose (false), debugInstrProcessing (false)
  {
    static bool staticInited = false;
    if (!staticInited)
    {
      TempHeap::Init();

      staticInited = true;
    }
  }

  bool DocumentPreprocessor::Initialize (iObjectRegistry* objReg)
  {
    objectreg = objReg;

    csRef<iVerbosityManager> verbosemgr (
      csQueryRegistry<iVerbosityManager> (objectreg));
    if (verbosemgr)
      do_verbose = verbosemgr->Enabled ("document.preprocessor")
        // Heritage from being a part of xmlshader
        || verbosemgr->Enabled ("renderer.shader");
    else
      do_verbose = false;

    csConfigAccess config (objectreg);
    debugInstrProcessing =
      config->GetBool ("Document.Preprocessor.DebugInstructionProcessing")
      // Heritage from being a part of xmlshader
      || config->GetBool ("Video.XMLShader.DebugInstructionProcessing");

    return true;
  }

  csPtr<iDocumentNode> DocumentPreprocessor::Process (iDocumentNode* node)
  {
    csRef<csWrappedDocumentNodeFactory> factory_ref;
    factory.Get (factory_ref);
    if (!factory_ref)
    {
      factory_ref.AttachNew (new csWrappedDocumentNodeFactory (this));
      factory = factory_ref;
    }
    csRef<iDocumentNode> wrapped_node;
    wrapped_node.AttachNew (factory_ref->CreateWrapperStatic (node));
    return csPtr<iDocumentNode> (wrapped_node);
  }
}
CS_PLUGIN_NAMESPACE_END(DocPreproc)
