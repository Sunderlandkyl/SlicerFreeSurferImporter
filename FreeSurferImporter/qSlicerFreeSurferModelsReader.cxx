/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QFileInfo>

// SlicerQt includes
#include "qSlicerFreeSurferModelsReader.h"

// Logic includes
#include "vtkSlicerModelsLogic.h"

// MRML includes
#include "vtkMRMLModelNode.h"
#include <vtkMRMLScene.h>
#include <vtkMRMLModelDisplayNode.h>

#include "vtkMRMLFreeSurferModelStorageNode.h"

// VTK includes
#include <vtkCacheManager.h>
#include <vtkSmartPointer.h>

#include <itksys/SystemTools.hxx>

//-----------------------------------------------------------------------------
class qSlicerFreeSurferModelsReaderPrivate
{
public:
  vtkSmartPointer<vtkSlicerModelsLogic> ModelsLogic;
};

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Models
qSlicerFreeSurferModelsReader::qSlicerFreeSurferModelsReader(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerFreeSurferModelsReaderPrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerFreeSurferModelsReader::~qSlicerFreeSurferModelsReader()
= default;

//-----------------------------------------------------------------------------
QString qSlicerFreeSurferModelsReader::description()const
{
  return "FreeSurfer model";
}

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerFreeSurferModelsReader::fileType()const
{
  return QString("FreeSurferModelFile");
}

//-----------------------------------------------------------------------------
QStringList qSlicerFreeSurferModelsReader::extensions()const
{
  return QStringList()
    << "FreeSurfer model (*.orig" " *.inflated *.sphere *.white *.smoothwm *.pial *.obj)";
}

//-----------------------------------------------------------------------------
bool qSlicerFreeSurferModelsReader::load(const IOProperties& properties)
{
  Q_D(qSlicerFreeSurferModelsReader);
  Q_ASSERT(properties.contains("fileName"));
  std::string fileName = properties["fileName"].toString().toStdString();

  this->setLoadedNodes(QStringList());
  if (this->mrmlScene() == nullptr || fileName.empty())
    {
    return nullptr;
    }

  vtkNew<vtkMRMLModelNode> modelNode;
  vtkNew<vtkMRMLFreeSurferModelStorageNode> fsStorageNode;
  fsStorageNode->SetFileName(fileName.c_str());

  // the model name is based on the file name (itksys call should work even if
  // file is not on disk yet)
  std::string name = itksys::SystemTools::GetFilenameName(fileName);

  // check to see which node can read this type of file
  if (!fsStorageNode->SupportedFileType(name.c_str()))
    {
    std::string errorMessage = "Couldn't read file: " + fileName;
    qCritical(errorMessage.c_str());
    return nullptr;
    }

  std::string baseName = itksys::SystemTools::GetFilenameWithoutExtension(name);
  std::string uname(this->mrmlScene()->GetUniqueNameByString(baseName.c_str()));
  modelNode->SetName(uname.c_str());
  this->mrmlScene()->AddNode(fsStorageNode.GetPointer());
 
  // Set the scene so that SetAndObserve[Display|Storage]NodeID can find the
  // node in the scene (so that DisplayNodes return something not empty)
  modelNode->SetScene(this->mrmlScene());
  modelNode->SetAndObserveStorageNodeID(fsStorageNode->GetID());
  this->mrmlScene()->AddNode(modelNode.GetPointer());

  // now set up the reading
  vtkDebugMacro("AddModel: calling read on the storage node");
  int retval = fsStorageNode->ReadData(modelNode.GetPointer());
  if (retval != 1)
    {
    std::string errorMessage = "Error reading " + fileName;
    qCritical(errorMessage.c_str());
    this->mrmlScene()->RemoveNode(fsStorageNode.GetPointer());
    this->mrmlScene()->RemoveNode(modelNode.GetPointer());
    return nullptr;
    }

  this->setLoadedNodes( QStringList(QString(modelNode->GetID())) );
  if (properties.contains("name"))
    {
    std::string uname = this->mrmlScene()->GetUniqueNameByString(
      properties["name"].toString().toLatin1());
    modelNode->SetName(uname.c_str());
    }
  modelNode->CreateDefaultDisplayNodes();
  return true;
}
