/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// FreeSurferImporter Logic includes
#include "vtkSlicerFreeSurferImporterLogic.h"

// MRML includes
#include <vtkMRMLModelNode.h>
#include <vtkMRMLModelStorageNode.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentationStorageNode.h>
#include <vtkMRMLVolumeArchetypeStorageNode.h>

// FreeSurferImporterMRML includes
#include "vtkMRMLFreeSurferModelOverlayStorageNode.h"
#include "vtkMRMLFreeSurferModelStorageNode.h"
#include "vtkMRMLFreeSurferProceduralColorNode.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <cassert>
#include <regex>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerFreeSurferImporterLogic);

//----------------------------------------------------------------------------
vtkSlicerFreeSurferImporterLogic::vtkSlicerFreeSurferImporterLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerFreeSurferImporterLogic::~vtkSlicerFreeSurferImporterLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::RegisterNodes()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
    {
    vtkErrorMacro("RegisterNodes: Invalid MRML scene!");
    return;
    }
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLFreeSurferModelOverlayStorageNode>::New());
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLFreeSurferModelStorageNode>::New());
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLFreeSurferProceduralColorNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}

//-----------------------------------------------------------------------------
vtkMRMLScalarVolumeNode* vtkSlicerFreeSurferImporterLogic::loadFreeSurferVolume(std::string fsDirectory, std::string name)
{
  std::string volumeFile = fsDirectory + name;
  vtkMRMLScalarVolumeNode* volumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLScalarVolumeNode"));
  volumeNode->SetName(name.c_str());
  volumeNode->AddDefaultStorageNode(volumeFile.c_str());

  vtkMRMLVolumeArchetypeStorageNode* volumeStorageNode = vtkMRMLVolumeArchetypeStorageNode::SafeDownCast(volumeNode->GetStorageNode());
  if (volumeStorageNode->ReadData(volumeNode))
    {
    volumeNode->CreateDefaultDisplayNodes();
    return volumeNode;
    }

  this->GetMRMLScene()->RemoveNode(volumeStorageNode);
  this->GetMRMLScene()->RemoveNode(volumeNode);
  return nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* vtkSlicerFreeSurferImporterLogic::loadFreeSurferSegmentation(std::string fsDirectory, std::string name)
{
  std::string segmentationFile = fsDirectory + name;
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLSegmentationNode"));
  if (!segmentationNode)
    {
    return nullptr;
    }
  segmentationNode->SetName(name.c_str());
  segmentationNode->AddDefaultStorageNode(segmentationFile.c_str());

  vtkMRMLSegmentationStorageNode* segmentationStorageNode = vtkMRMLSegmentationStorageNode::SafeDownCast(segmentationNode->GetStorageNode());
  if (segmentationStorageNode && segmentationStorageNode->ReadData(segmentationNode))
    {
    this->applyFreeSurferSegmentationLUT(segmentationNode);
    return segmentationNode;
    }

  this->GetMRMLScene()->RemoveNode(segmentationStorageNode);
  this->GetMRMLScene()->RemoveNode(segmentationNode);
  return nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLModelNode* vtkSlicerFreeSurferImporterLogic::loadFreeSurferModel(std::string fsDirectory, std::string name)
{
  std::string surfFile = fsDirectory + name;
  vtkMRMLModelNode* surfNode = vtkMRMLModelNode::SafeDownCast(this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLModelNode"));
  if (!surfNode)
    {
    return nullptr;
    }
  surfNode->SetName(name.c_str());

  vtkMRMLFreeSurferModelStorageNode* surfStorageNode = vtkMRMLFreeSurferModelStorageNode::SafeDownCast(
    this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLFreeSurferModelStorageNode"));
  if (surfStorageNode)
    {
    surfNode->SetAndObserveStorageNodeID(surfStorageNode->GetID());
    surfStorageNode->SetFileName(surfFile.c_str());
    if (surfStorageNode->ReadData(surfNode))
      {
      return surfNode;
      }
    }

  this->GetMRMLScene()->RemoveNode(surfStorageNode);
  this->GetMRMLScene()->RemoveNode(surfNode);
  return nullptr;
}

//-----------------------------------------------------------------------------
bool vtkSlicerFreeSurferImporterLogic::loadFreeSurferScalarOverlay(std::string fsDirectory, std::string name, std::vector<vtkMRMLModelNode*> modelNodes)
{
  std::string hemisphereName = vtksys::SystemTools::GetFilenameWithoutExtension(name);

  vtkMRMLFreeSurferModelOverlayStorageNode* overlayStorageNode = vtkMRMLFreeSurferModelOverlayStorageNode::SafeDownCast(
    this->GetMRMLScene()->AddNewNodeByClass("vtkMRMLFreeSurferModelOverlayStorageNode"));
  if (!overlayStorageNode)
    {
    return false;
    }
  std::string overlayFile = fsDirectory + name;
  overlayStorageNode->SetFileName(overlayFile.c_str());

  bool success = true;
  int numberOfOverlayLoaded = 0;
  for (vtkMRMLModelNode* modelNode : modelNodes)
    {
    if (!modelNode->GetName())
      {
      continue;
      }

    std::string modelNodeHemisphereName = vtksys::SystemTools::GetFilenameWithoutExtension(modelNode->GetName());
    if (modelNodeHemisphereName != hemisphereName)
      {
      continue;
      }

    // Scalar overlay is already loaded for this model
    if (modelNode->HasCellScalarName(name.c_str()))
      {
      continue;
      }

    if (!overlayStorageNode->ReadData(modelNode))
      {
      success = false;
      continue;
      }
    numberOfOverlayLoaded += 1;
    }

  this->GetMRMLScene()->RemoveNode(overlayStorageNode);
  if (numberOfOverlayLoaded == 0)
    {
    success = false;
    }
  return success;
}

//-----------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::transformFreeSurferModelToRAS(vtkMRMLModelNode* modelNode, vtkMRMLScalarVolumeNode* origVolumeNode)
{
  if (!modelNode || !origVolumeNode)
    {
    return;
    }

  int extent[6] = { 0 };
  origVolumeNode->GetImageData()->GetExtent(extent);

  int dimensions[3] = { 0 };
  origVolumeNode->GetImageData()->GetDimensions(dimensions);

  double center[4] = { 0, 0, 0, 1 };
  for (int i = 0; i < 3; ++i)
    {
    center[i] = extent[2 * i] + std::ceil((dimensions[i] / 2.0));
    }

  vtkNew<vtkMatrix4x4> ijkToRAS;
  origVolumeNode->GetIJKToRASMatrix(ijkToRAS);
  ijkToRAS->MultiplyPoint(center, center);

  vtkNew<vtkTransform> transform;
  transform->Translate(center);

  vtkNew<vtkTransformPolyDataFilter> transformer;
  transformer->SetTransform(transform);
  transformer->SetInputData(modelNode->GetPolyData());
  transformer->Update();
  modelNode->GetPolyData()->ShallowCopy(transformer->GetOutput());
  modelNode->GetPolyData()->Modified();
}

//-----------------------------------------------------------------------------
struct SegmentInfo
{
  std::string name = "Unknown";
  double color[3] = { 0.5 };
};

//-----------------------------------------------------------------------------
void vtkSlicerFreeSurferImporterLogic::applyFreeSurferSegmentationLUT(vtkMRMLSegmentationNode* segmentationNode)
{
  if (!segmentationNode)
    {
    return;
    }
  MRMLNodeModifyBlocker blocker(segmentationNode);

  std::string sharedDirectory = this->GetModuleShareDirectory();
  std::string lutFilename = "FreeSurferColorLUT.txt";

  std::string lutDirectory = sharedDirectory + "/" + lutFilename;

  std::ifstream lutFile;
  lutFile.open(lutDirectory);
  if (!lutFile.is_open())
    {
    return;
    }

  std::map<int, SegmentInfo> segmentInfoMap;

  std::string line;
  while (std::getline(lutFile, line))
    {
    line = std::regex_replace(line, std::regex("^ +| +$|( ) +"), "$1");
    if (line.empty())
      {
      continue;
      }
    if (line[0] == '#')
      {
      continue;
      }

    std::vector<std::string> tokens;
    std::stringstream ss(line);;
    std::string token;
    while (std::getline(ss, token, ' '))
      {
      tokens.push_back(token);
      }
    if (tokens.size() != 6)
      {
      continue;
      }

    int value = vtkVariant(tokens[0]).ToInt();
    SegmentInfo info;
    info.name = tokens[1];
    info.color[0] = vtkVariant(tokens[2]).ToInt() / 255.0;
    info.color[1] = vtkVariant(tokens[3]).ToInt() / 255.0;
    info.color[2] = vtkVariant(tokens[4]).ToInt() / 255.0;
    segmentInfoMap[value] = info;
    }
  lutFile.close();

  vtkSegmentation* segmentation = segmentationNode->GetSegmentation();
  for (int i = 0; i < segmentation->GetNumberOfSegments(); ++i)
    {
    vtkSegment* segment = segmentation->GetNthSegment(i);
    SegmentInfo info = segmentInfoMap[segment->GetLabelValue()];
    segment->SetName(info.name.c_str());
    segment->SetColor(info.color);
    }
}

////-------------------------------------------------------------------------
//void vtkSlicerVolumesLogic::TranslateFreeSurferRegistrationMatrixIntoSlicerRASToRASMatrix(vtkMRMLVolumeNode* V1Node,
//  vtkMRMLVolumeNode* V2Node,
//  vtkMatrix4x4* FSRegistrationMatrix,
//  vtkMatrix4x4* RAS2RASMatrix)
//{
//  if (V1Node && V2Node && FSRegistrationMatrix && RAS2RASMatrix)
//  {
//    RAS2RASMatrix->Zero();
//
//    //
//    // Looking for RASv1_To_RASv2:
//    //
//    //---
//    //
//    // In Slicer:
//    // [ IJKv1_To_IJKv2] = [ RAS_To_IJKv2 ]  [ RASv1_To_RASv2 ] [ IJK_To_RASv1 ] [i,j,k]transpose
//    //
//    // In FreeSurfer:
//    // [ IJKv1_To_IJKv2] = [FStkRegVox_To_RASv2 ]inverse [ FSRegistrationMatrix] [FStkRegVox_To_RASv1 ] [ i,j,k] transpose
//    //
//    //----
//    //
//    // So:
//    // [FStkRegVox_To_RASv2 ] inverse [ FSRegistrationMatrix] [FStkRegVox_To_RASv1 ] =
//    // [ RAS_To_IJKv2 ]  [ RASv1_To_RASv2 ] [ IJKv1_2_RAS ]
//    //
//    //---
//    //
//    // Below use this shorthand:
//    //
//    // S = FStkRegVox_To_RASv2
//    // T = FStkRegVox_To_RASv1
//    // N = RAS_To_IJKv2
//    // M = IJK_To_RASv1
//    // R = FSRegistrationMatrix
//    // [Sinv]  [R]  [T] = [N]  [RASv1_To_RASv2]  [M];
//    //
//    // So this is what we'll compute and use in Slicer instead
//    // of the FreeSurfer register.dat matrix:
//    //
//    // [Ninv]  [Sinv]  [R]  [T]  [Minv]  = RASv1_To_RASv2
//    //
//    // I think we need orientation in FreeSurfer: nothing in the tkRegVox2RAS
//    // handles scanOrder. The tkRegVox2RAS = IJKToRAS matrix for a coronal
//    // volume. But for an Axial volume, these two matrices are different.
//    // How do we compute the correct orientation for FreeSurfer Data?
//
//    vtkNew<vtkMatrix4x4> T;
//    vtkNew<vtkMatrix4x4> S;
//    vtkNew<vtkMatrix4x4> Sinv;
//    vtkNew<vtkMatrix4x4> M;
//    vtkNew<vtkMatrix4x4> Minv;
//    vtkNew<vtkMatrix4x4> N;
//    vtkNew<vtkMatrix4x4> Ninv;
//
//    //--
//    // compute FreeSurfer tkRegVox2RAS for V1 volume
//    //--
//    ComputeTkRegVox2RASMatrix(V1Node, T.GetPointer());
//
//    //--
//    // compute FreeSurfer tkRegVox2RAS for V2 volume
//    //--
//    ComputeTkRegVox2RASMatrix(V2Node, S.GetPointer());
//
//    // Probably a faster way to do these things?
//    vtkMatrix4x4::Invert(S.GetPointer(), Sinv.GetPointer());
//    V1Node->GetIJKToRASMatrix(M.GetPointer());
//    V2Node->GetRASToIJKMatrix(N.GetPointer());
//    vtkMatrix4x4::Invert(M.GetPointer(), Minv.GetPointer());
//    vtkMatrix4x4::Invert(N.GetPointer(), Ninv.GetPointer());
//
//    //    [Ninv]  [Sinv]  [R]  [T]  [Minv]
//    vtkMatrix4x4::Multiply4x4(T.GetPointer(), Minv.GetPointer(), RAS2RASMatrix);
//    vtkMatrix4x4::Multiply4x4(FSRegistrationMatrix, RAS2RASMatrix, RAS2RASMatrix);
//    vtkMatrix4x4::Multiply4x4(Sinv.GetPointer(), RAS2RASMatrix, RAS2RASMatrix);
//    vtkMatrix4x4::Multiply4x4(Ninv.GetPointer(), RAS2RASMatrix, RAS2RASMatrix);
//  }
//}
