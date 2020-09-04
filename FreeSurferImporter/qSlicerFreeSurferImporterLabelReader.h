/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.

==============================================================================*/

#ifndef __qSlicerFreeSurferImporterLabelReader_h
#define __qSlicerFreeSurferImporterLabelReader_h

// SlicerFreeSurferImporter includes
#include "qSlicerFreeSurferImporterModuleExport.h"

// SlicerQt includes
#include "qSlicerFileReader.h"

class qSlicerFreeSurferImporterLabelReaderPrivate;
class vtkSlicerFreeSurferImporterLogic;

//-----------------------------------------------------------------------------
class Q_SLICER_QTMODULES_FREESURFERIMPORTER_EXPORT qSlicerFreeSurferImporterLabelReader
  : public qSlicerFileReader
{
  Q_OBJECT
public:
  typedef qSlicerFileReader Superclass;
  qSlicerFreeSurferImporterLabelReader(vtkSlicerFreeSurferImporterLogic* logic, QObject* parent = nullptr);
  ~qSlicerFreeSurferImporterLabelReader() override;

  void setLogic(vtkSlicerFreeSurferImporterLogic* newFreeSurferImporterLogic);
  vtkSlicerFreeSurferImporterLogic* Logic() const;

  QString description()const override;
  IOFileType fileType()const override;
  QStringList extensions()const override;
  qSlicerIOOptions* options()const override;

  bool load(const IOProperties& properties) override;

protected:
  QScopedPointer<qSlicerFreeSurferImporterLabelReaderPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerFreeSurferImporterLabelReader);
  Q_DISABLE_COPY(qSlicerFreeSurferImporterLabelReader);
};

#endif
