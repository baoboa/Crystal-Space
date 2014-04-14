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

#ifndef __IVARIA_DOCPREPROC_H__
#define __IVARIA_DOCPREPROC_H__

/**\file
 * Document preprocessor plugin interface
 */

struct iDocumentNode;

namespace CS
{
  namespace DocSystem
  {
    /**
     * Interface to document preprocessor plugin.
     * This applies a preprocessing step featuring &ldquo;templates&rdquo; (comparable
     * to C++ preprocessor macros), &ldquo;defines&rdquo; (comparable to C++ preprocessor
     * conditions) and &ldquo;generation&rdquo; (like a <tt>for</tt>-loop for nodes).
     *
     * For a reference, see the &ldquo;Shader Processing Instructions&rdquo;
     * section in:
     * http://crystalspace3d.org/docs/online/manual/Shader-Conditions-and-Processing-Instructions-Reference.html
     * (Note: &ldquo;Shader Conditions&rdquo; support is \em not handled by
     * the preprocessor plugin!)
     *
     * Main implementor of this interface:
     * - Document preprocessor plugin (crystalspace.document.preprocessor)
     */
    struct iDocumentPreprocessor : public virtual iBase
    {
      SCF_INTERFACE(CS::DocSystem::iDocumentPreprocessor, 0, 0, 1);

      /**
       * Preprocess a document node and it's children.
       * Returns a new node instance with the preprocessed result.
       * \remarks The original document node will still be referred to;
       *   changing it and simulatenously keeping a preprocessed node
       *   around will have undefined results!
       */
      virtual csPtr<iDocumentNode> Process (iDocumentNode* doc) = 0;
    };
  } // namespace DocSystem
} // namespace CS

#endif // __IVARIA_DOCPREPROC_H__
