#include "LayerFCD.h"
#include "LayerPropertyFCD.h"
#include "vtkRenderer.h"
#include "vtkImageReslice.h"
#include "vtkImageActor.h"
#include "vtkPolyDataMapper.h"
#include "vtkSmartPointer.h"
#include "vtkMatrix4x4.h"
#include "vtkImageMapToColors.h"
#include "vtkImageFlip.h"
#include "vtkTransform.h"
#include "vtkPlaneSource.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkTexture.h"
#include "vtkImageActor.h"
#include "vtkActor.h"
#include "vtkRGBAColorTransferFunction.h"
#include "vtkLookupTable.h"
#include "vtkProperty.h"
#include "vtkFreesurferLookupTable.h"
#include "vtkImageChangeInformation.h"
#include "LayerPropertyROI.h"
#include "LayerMRI.h"
#include "LayerPropertyMRI.h"
#include "FSVolume.h"
#include <QDebug>
#include <QTimer>

LayerFCD::LayerFCD(LayerMRI* layerMRI, QObject *parent) : LayerVolumeBase(parent),
  m_fcd(NULL)
{
  m_strTypeNames.push_back( "FCD" );
  m_sPrimaryType = "FCD";
  mProperty = new LayerPropertyFCD(this);
  for ( int i = 0; i < 3; i++ )
  {
    m_dSlicePosition[i] = 0;
    m_sliceActor2D[i] = vtkSmartPointer<vtkImageActor>::New();
    m_sliceActor3D[i] = vtkSmartPointer<vtkImageActor>::New();
    m_sliceActor2D[i]->InterpolateOff();
    m_sliceActor3D[i]->InterpolateOff();
  }

  mProperty = new LayerPropertyFCD( this );

  m_layerSource = layerMRI;
  GetProperty()->SetLookupTable(layerMRI->GetProperty()->GetLUTTable());
  m_imageDataRef = layerMRI->GetImageData();
  if ( m_layerSource )
  {
    SetWorldOrigin( m_layerSource->GetWorldOrigin() );
    SetWorldVoxelSize( m_layerSource->GetWorldVoxelSize() );
    SetWorldSize( m_layerSource->GetWorldSize() );

    m_imageData = vtkSmartPointer<vtkImageData>::New();
    // m_imageData->DeepCopy( m_layerSource->GetRASVolume() );

    m_imageData->SetNumberOfScalarComponents( 1 );
    m_imageData->SetScalarTypeToUnsignedChar();
    m_imageData->SetOrigin( GetWorldOrigin() );
    m_imageData->SetSpacing( GetWorldVoxelSize() );
    m_imageData->SetDimensions( ( int )( m_dWorldSize[0] / m_dWorldVoxelSize[0] + 0.5 ),
                                ( int )( m_dWorldSize[1] / m_dWorldVoxelSize[1] + 0.5 ),
                                ( int )( m_dWorldSize[2] / m_dWorldVoxelSize[2] + 0.5 ) );
    m_imageData->AllocateScalars();
    void* ptr = m_imageData->GetScalarPointer();
    int* nDim = m_imageData->GetDimensions();
    // cout << nDim[0] << ", " << nDim[1] << ", " << nDim[2] << endl;
    memset( ptr, 0, m_imageData->GetScalarSize() * nDim[0] * nDim[1] * nDim[2] );
    InitializeActors();
  }

  LayerPropertyFCD* p = GetProperty();
  connect( p, SIGNAL(ColorMapChanged()), this, SLOT(UpdateColorMap()) );
  connect( p, SIGNAL(OpacityChanged(double)), this, SLOT(UpdateOpacity()) );
  connect( p, SIGNAL(ThicknessThresholdChanged(double)), this, SLOT(Recompute()));
  connect( p, SIGNAL(SigmaChanged(double)), this, SLOT(Recompute()));
  connect( p, SIGNAL(MinAreaChanged(double)), this, SLOT(Recompute()));
}

LayerFCD::~LayerFCD()
{
  //if (m_fcd) ;
}

bool LayerFCD::Load(const QString &subdir, const QString &subject)
{
  m_sSubjectDir = subdir;
  m_sSubject = subject;

  return LoadFromFile();
}

bool LayerFCD::LoadFromFile()
{
  m_fcd = ::FCDloadData(m_sSubjectDir.toAscii().data(), m_sSubject.toAscii().data());
  Recompute();

  return (m_fcd != NULL);
}

void LayerFCD::Recompute()
{
  if (m_fcd)
  {
    ::FCDcomputeThicknessLabels(m_fcd, GetProperty()->GetThicknessThreshold(), GetProperty()->GetSigma(), GetProperty()->GetMinArea());
    UpdateRASImage(m_imageData);
    emit LabelsChanged();
    emit ActorUpdated();
  }
}

void LayerFCD::UpdateRASImage( vtkImageData* rasImage)
{
  if (!m_fcd)
    return;

  int n[3];
  double pos[3];
  int* dim = rasImage->GetDimensions();
  memset( rasImage->GetScalarPointer(),
          0,
          dim[0] * dim[1] * dim[2] * rasImage->GetScalarSize() );
  if ( m_fcd->nlabels == 0 )
  {
    cout << "No labels found\n";
    return;
  }

  for (int cnt = 0; cnt < m_fcd->nlabels; cnt++)
  {
    LABEL* label = m_fcd->labels[cnt];
    FSVolume* ref_vol = m_layerSource->GetSourceVolume();
    for ( int i = 0; i < label->n_points; i++ )
    {
      pos[0] = label->lv[i].x;
      pos[1] = label->lv[i].y;
      pos[2] = label->lv[i].z;
      if ( label->coords == LABEL_COORDS_VOXEL )
      {
        MRIvoxelToWorld(ref_vol->GetMRI(), pos[0], pos[1], pos[2], pos, pos+1, pos+2);
      }
      else if (label->coords == LABEL_COORDS_TKREG_RAS)
      {
        ref_vol->TkRegToNativeRAS( pos, pos );
      }
      ref_vol->NativeRASToRAS( pos, pos );
      ref_vol->RASToTargetIndex( pos, n );
      if ( n[0] >= 0 && n[0] < dim[0] && n[1] >= 0 && n[1] < dim[1] &&
           n[2] >= 0 && n[2] < dim[2] )
      {
        rasImage->SetScalarComponentFromFloat( n[0], n[1], n[2], 0, label->lv[i].vno );
      }
    }
  }
}


void LayerFCD::InitializeActors()
{
  if ( m_layerSource == NULL )
  {
    return;
  }

  for ( int i = 0; i < 3; i++ )
  {
    // The reslice object just takes a slice out of the volume.
    //
    mReslice[i] = vtkSmartPointer<vtkImageReslice>::New();
    mReslice[i]->SetInput( m_imageData );
//  mReslice[i]->SetOutputSpacing( sizeX, sizeY, sizeZ );
    mReslice[i]->BorderOff();

    // This sets us to extract slices.
    mReslice[i]->SetOutputDimensionality( 2 );

    // This will change depending what orienation we're in.
    mReslice[i]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 1, 0,
        0, 0, 1 );

    // This will change to select a different slice.
    mReslice[i]->SetResliceAxesOrigin( 0, 0, 0 );
    //
    // Image to colors using color table.
    //
    mColorMap[i] = vtkSmartPointer<vtkImageMapToColors>::New();
    mColorMap[i]->SetLookupTable( GetProperty()->GetLookupTable() );
    mColorMap[i]->SetInputConnection( mReslice[i]->GetOutputPort() );
    mColorMap[i]->SetOutputFormatToRGBA();
    mColorMap[i]->PassAlphaToOutputOn();

    //
    // Prop in scene with plane mesh and texture.
    //
    m_sliceActor2D[i]->SetInput( mColorMap[i]->GetOutput() );
    m_sliceActor3D[i]->SetInput( mColorMap[i]->GetOutput() );

    // Set ourselves up.
    this->OnSlicePositionChanged( i );
  }

  this->UpdateOpacity();
  this->UpdateColorMap();
}

void LayerFCD::UpdateOpacity()
{
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->SetOpacity( GetProperty()->GetOpacity() );
    m_sliceActor3D[i]->SetOpacity( GetProperty()->GetOpacity() );
  }

  emit ActorUpdated();
}

void LayerFCD::UpdateColorMap ()
{
  for ( int i = 0; i < 3; i++ )
  {
    mColorMap[i]->SetLookupTable( GetProperty()->GetLookupTable() );
  }
  // m_sliceActor2D[i]->GetProperty()->SetColor(1, 0, 0);

  emit ActorUpdated();
}

void LayerFCD::Append2DProps( vtkRenderer* renderer, int nPlane )
{
  renderer->AddViewProp( m_sliceActor2D[nPlane] );
}

void LayerFCD::Append3DProps( vtkRenderer* renderer, bool* bSliceVisibility )
{
  for ( int i = 0; i < 3; i++ )
  {
    if ( bSliceVisibility == NULL || bSliceVisibility[i] )
    {
      renderer->AddViewProp( m_sliceActor3D[i] );
    }
  }
}

void LayerFCD::OnSlicePositionChanged( int nPlane )
{
  if ( !m_layerSource )
  {
    return;
  }

  vtkSmartPointer<vtkMatrix4x4> matrix =
    vtkSmartPointer<vtkMatrix4x4>::New();
  matrix->Identity();
  switch ( nPlane )
  {
  case 0:
    m_sliceActor2D[0]->PokeMatrix( matrix );
    m_sliceActor2D[0]->SetPosition( m_dSlicePosition[0], 0, 0 );
    m_sliceActor2D[0]->RotateX( 90 );
    m_sliceActor2D[0]->RotateY( -90 );
    m_sliceActor3D[0]->PokeMatrix( matrix );
    m_sliceActor3D[0]->SetPosition( m_dSlicePosition[0], 0, 0 );
    m_sliceActor3D[0]->RotateX( 90 );
    m_sliceActor3D[0]->RotateY( -90 );

    // Putting negatives in the reslice axes cosines will flip the
    // image on that axis.
    mReslice[0]->SetResliceAxesDirectionCosines( 0, -1, 0,
        0, 0, 1,
        1, 0, 0 );
    mReslice[0]->SetResliceAxesOrigin( m_dSlicePosition[0], 0, 0  );
    mReslice[0]->Modified();
    break;
  case 1:
    m_sliceActor2D[1]->PokeMatrix( matrix );
    m_sliceActor2D[1]->SetPosition( 0, m_dSlicePosition[1], 0 );
    m_sliceActor2D[1]->RotateX( 90 );
    // m_sliceActor2D[1]->RotateY( 180 );
    m_sliceActor3D[1]->PokeMatrix( matrix );
    m_sliceActor3D[1]->SetPosition( 0, m_dSlicePosition[1], 0 );
    m_sliceActor3D[1]->RotateX( 90 );
    // m_sliceActor3D[1]->RotateY( 180 );

    // Putting negatives in the reslice axes cosines will flip the
    // image on that axis.
    mReslice[1]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 0, 1,
        0, 1, 0 );
    mReslice[1]->SetResliceAxesOrigin( 0, m_dSlicePosition[1], 0 );
    mReslice[1]->Modified();
    break;
  case 2:
    m_sliceActor2D[2]->SetPosition( 0, 0, m_dSlicePosition[2] );
    // m_sliceActor2D[2]->RotateY( 180 );
    m_sliceActor3D[2]->SetPosition( 0, 0, m_dSlicePosition[2] );
    // m_sliceActor3D[2]->RotateY( 180 );

    mReslice[2]->SetResliceAxesDirectionCosines( 1, 0, 0,
        0, 1, 0,
        0, 0, 1 );
    mReslice[2]->SetResliceAxesOrigin( 0, 0, m_dSlicePosition[2]  );
    mReslice[2]->Modified();
    break;
  }
}

void LayerFCD::SetVisible( bool bVisible )
{
  for ( int i = 0; i < 3; i++ )
  {
    m_sliceActor2D[i]->SetVisibility( bVisible ? 1 : 0 );
    m_sliceActor3D[i]->SetVisibility( bVisible ? 1 : 0 );
  }
  LayerVolumeBase::SetVisible(bVisible);
}

bool LayerFCD::IsVisible()
{
  return m_sliceActor2D[0]->GetVisibility() > 0;
}

bool LayerFCD::HasProp( vtkProp* prop )
{
  for ( int i = 0; i < 3; i++ )
  {
    if ( prop == m_sliceActor2D[i].GetPointer() || prop == m_sliceActor3D[i].GetPointer() )
    {
      return true;
    }
  }
  return false;
}

void LayerFCD::GetLabelCentroidPosition(int nLabelIndex, double *pos)
{
  if (nLabelIndex >= m_fcd->nlabels)
    return;

  LABEL* label = m_fcd->labels[nLabelIndex];
  if (label->n_points > 0)
  {
    double x = 0, y = 0, z = 0;
    for ( int i = 0; i < label->n_points; i++ )
    {
      x += label->lv[i].x;
      y += label->lv[i].y;
      z += label->lv[i].z;
    }
    pos[0] = x / label->n_points;
    pos[1] = y / label->n_points;
    pos[2] = z / label->n_points;

    FSVolume* ref_vol = m_layerSource->GetSourceVolume();
    if ( label->coords == LABEL_COORDS_VOXEL )
    {
      MRIvoxelToWorld(ref_vol->GetMRI(), pos[0], pos[1], pos[2], pos, pos+1, pos+2);
    }
    else if (label->coords == LABEL_COORDS_TKREG_RAS)
    {
      ref_vol->TkRegToNativeRAS( pos, pos );
    }
    ref_vol->NativeRASToRAS( pos, pos );
    m_layerSource->RASToTarget(pos, pos);
  }
}