/**
 * @file Registration.h
 * @brief A class to compute a robust symmetric registration
 *
 */

/*
 * Original Author: Martin Reuter
 * CVS Revision Info:
 *    $Author: mreuter $
 *    $Date: 2012/05/25 22:57:22 $
 *    $Revision: 1.53 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */

// written by Martin Reuter
// Nov. 4th ,2008
//

#ifndef Registration_H
#define Registration_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "matrix.h"
#include "error.h"
#include "mri.h"
#ifdef __cplusplus
}
#endif

#include <utility>
#include <string>
#include <vector>
#include <typeinfo>
#include <iostream>
#include <sstream>
#include <vnl/vnl_vector.h>
#include <vnl/vnl_matrix.h>
#include <vnl/vnl_matrix_fixed.h>
#include <vnl/vnl_inverse.h>
#include <vnl/vnl_matlab_print.h>
#include <vcl_iostream.h>

#include "MyMatrix.h"
#include "MyMRI.h"


// forward declaration
template <class T> class RegistrationStep;

class Registration
{
 template <class T> friend class RegistrationStep;
public:

enum Cost
{
  LS = 0,
  ROB,
  MI,
  NMI,
  ECC,
  NCC,
  SCR
};

  Registration(): sat(-1),iscale(false),transonly(false),rigid(true),
      rtype(1),subsamplesize(-1),minsize(-1),maxsize(-1),debug(0),verbose(1),initorient(false),
      inittransform(true),initscaling(false),highit(-1),mri_source(NULL),mri_target(NULL),iscaleinit(1.0),
      iscalefinal(1.0),doubleprec(false),wlimit(0.175),symmetry(true),sampletype(SAMPLE_TRILINEAR),
      resample(false),costfun(ROB),	mri_weights(NULL), mri_hweights(NULL),mri_indexing(NULL)
  {};
  Registration(MRI * s, MRI *t): sat(-1),iscale(false),transonly(false),rigid(true),
      rtype(1),subsamplesize(-1),minsize(-1),maxsize(-1),debug(0),verbose(1),initorient(false),
      inittransform(true),initscaling(false),highit(-1),mri_source(MRIcopy(s,NULL)),mri_target(MRIcopy(t,NULL)),
      iscaleinit(1.0),iscalefinal(1.0),doubleprec(false),wlimit(0.175),symmetry(true),
      sampletype(SAMPLE_TRILINEAR),resample(false),costfun(ROB),
			mri_weights(NULL),mri_hweights(NULL),mri_indexing(NULL)
  {};

  virtual ~Registration();

  void clear(); // initialize registration (keep source and target and gauss pyramid)
  void freeGPS() {freeGaussianPyramid(gpS);};
  void freeGPT() {freeGaussianPyramid(gpT);};

  // Set parameters:
  void setTransonly(bool r)
  {
    transonly = r;
  };
  void setRigid(bool r)
  {
    rigid = r;
  };
  void setSaturation(double d)
  {
    sat = d;
  };
  void setHighit(int i)
  {
    highit = i;
  };
  void setDebug(int d)
  {
    debug = d;
    if (d>0) verbose = 2;
  };
  void setVerbose(int i)
	// 0 very quiet, 1 default, 2 detail
  {
    if (i < 0) i=0;
    else if (i > 2) i=2;
    verbose = i;
  };
  void setIscale(bool i)
  {
    iscale = i;
  };
  void setIscaleInit(double d)
  {
    iscaleinit = d;
  };
  void setRtype(int r)
  {
    rtype = r;
  };
  void setMinitOrig(const vnl_matrix < double > & m)
  {
    Minit =m; // this is for the original volumes (not in resampled space!)
  };
  void setSource (MRI * s, bool conform = false, bool keeptype = false);
  void setTarget (MRI * t, bool conform = false, bool keeptype = false);
  void setSourceAndTarget(MRI * s, MRI * t, bool keeptype = false);
  void setSubsampleSize (int sss)
  {
    subsamplesize = sss;
  };
  void setMinSize (int s)
  {
    minsize = s;
  };
  void setMaxSize (int s)
  {
    maxsize = s;
  };
  void setName(const std::string &n);
	
	// if inittransform is true additionally use orientation?
  void setInitOrient(bool io)
  {
    initorient = io;
  };
	
	// if true (default) automatically initialize transform
  void setInitTransform(bool it)
  {
    inittransform = it;
  };
  void setInitScaling(bool it)
  {
    initscaling = it;
  };
  void setDoublePrec(bool b)
  {
    doubleprec = b;
  };
  void setWLimit( double d)
  {
    wlimit = d;
  };
  void setSymmetry( bool b)
  {
    symmetry = b;
  };
  void setSampleType(int st)
  {
    switch (st)
    {
    case SAMPLE_TRILINEAR:
      break;
    default:
      std::cout << "ERROR Registration:setSampleType: " << st << " not supported type!" << std::endl;
      exit(1);
    }
    sampletype = st;
  }
  void setCost( Cost c)
  {
    costfun = c;
  };

  bool isIscale()
  {
    return iscale;
  };
  std::string  getName()
  {
    return name;
  };
  MRI * getWeights()
  {
    return mri_weights;
  };
  MRI * getHWeights()
  {
    return mri_hweights;
  };
  MRI * getHalfWayGeom()
  {
    return mri_weights;
  };
  std::pair <vnl_matrix_fixed< double, 4, 4 > , vnl_matrix_fixed < double, 4, 4 > > getHalfWayMaps();

  double findSaturation ();
  //std::pair <MATRIX*, double> computeIterativeRegSat( int n,double epsit,MRI * mriS=NULL, MRI* mriT=NULL, MATRIX* Minit = NULL, double iscaleinit = 1.0);

  // compute registration
  void computeIterativeRegistration(int n,double epsit);
  void computeMultiresRegistration (int stopres, int n,double epsit);

  // get final transform (might be different from mfinal due to possible resampling)
  vnl_matrix_fixed < double , 4 , 4 >  getFinalVox2Vox ();
  double getFinalIscale() {return iscalefinal;};

  double estimateIScale(MRI *mriS, MRI *mriT);

//   bool warpSource(const std::string & fname, MATRIX* M = NULL, double is = -1);
//   bool warpSource(MRI* orig, MRI* target, const std::string &fname, MATRIX* M = NULL, double is = -1);

  // testing
  void testRobust(const std::string & fname, int testno);

//  double computeSatEstimate (int reslevel, int n,double epsit, MRI * mriS=NULL, MRI* mriT=NULL, MATRIX* mi=NULL, double scaleinit=1.0 );

  // return centroid (of input, not of resampled internal version)
  //std::vector < double > getCentroidS();
  //std::vector < double > getCentroidT();
  vnl_vector_fixed < double, 4 > getCentroidS();
  vnl_vector_fixed < double, 4 > getCentroidT();
  vnl_vector_fixed < double, 4 > getCentroidSinT();
	

protected:

  //   returns weights:
 // std::pair < vnl_vector <double >, MRI* > computeRegistrationStepW(MRI * mriS = NULL, MRI* mriT=NULL);
  //   returns param vector:
//  vnl_vector <double > computeRegistrationStepP(MRI * mriS, MRI* mriT);
  //   returns 4x4 matrix and iscale:
//  std::pair < vnl_matrix_fixed <double,4,4 >, double> computeRegistrationStep(MRI * mriS = NULL, MRI* mriT = NULL);

  // IterativeRegistrationHelper
  virtual void computeIterativeRegistration(int n,double epsit,MRI * mriS, MRI* mriT, const vnl_matrix < double > &Minit, double iscaleinit);	
	template < class T > void iterativeRegistrationHelper( int nmax,double epsit, MRI * mriS, MRI* mriT, const vnl_matrix < double >& m, double scaleinit);
  bool flipInputs();
  bool reorientSource();

  //conversion
  //std::pair < vnl_matrix_fixed <double,4,4 >, double > convertP2Md(const vnl_vector <double >& p);
  //std::pair < MATRIX*, double > convertP2MATRIXd(MATRIX* p);
	
	//MRI * applyParams(MRI * mri_in, const vnl_vector<double>& p, MRI * mri_dst=NULL, bool inverse=false);

  // transform minit into resampled space
	vnl_matrix < double > getMinitResampled();

  // initial registration using moments
  vnl_matrix_fixed < double,4,4>  initializeTransform(MRI *mri_in, MRI *mri_ref);
  int init_scaling(MRI *mri_in, MRI *mri_ref, MATRIX *m_L); // NOT TESTED !!!!!!!
  
  // checks
  bool checkSqrtM(const vnl_matrix_fixed <double,4,4 > & sqrtM, bool rigid);

  // resample both images (or if not symmetric only src) to new space
  // also return half way transforms
  void mapToNewSpace(const vnl_matrix_fixed<double , 4, 4>& M, double iscaleval, 
                                 MRI * mriS, MRI* mriT, MRI *& mriSwarp, MRI*& mriTwarp,
                                 vnl_matrix_fixed<double , 4, 4>& mh,
                                 vnl_matrix_fixed<double , 4, 4>& mhi);

  double sat;
  bool iscale;
  bool transonly;
  bool rigid;
  int rtype;
  int subsamplesize;
  int minsize;
  int maxsize;
  std::string name;
  std::string nbase;
  int debug;
  int verbose;
  bool initorient;
  bool inittransform;
  bool initscaling;
  int highit;

  MRI * mri_source;
  std::vector < MRI* > gpS;
  std::vector < double > centroidS;
  MRI * mri_target;
  std::vector < MRI* > gpT;
  std::vector < double > centroidT;
  vnl_matrix < double >  Minit;
  vnl_matrix < double >  Mfinal;
	double iscaleinit;
  double iscalefinal;
  bool doubleprec;
  double wlimit;
  bool symmetry;
  int sampletype;

  bool resample;
  vnl_matrix < double >  Rsrc;
  vnl_matrix < double >  Rtrg;

  vnl_matrix < double> mov2weights;
  vnl_matrix < double> dst2weights;

  Cost costfun;

private:

  // construct Ab and R:
  //MATRIX* constructR(MATRIX* p);
  //std::pair < MATRIX*, VECTOR* > constructAb(MRI *mriS, MRI *mriT);
  //std::pair < vnl_matrix <double>, vnl_vector <double> > constructAb(MRI *mriS, MRI *mriT);
//  void constructAb(MRI *mriS, MRI *mriT, vnl_matrix < double > &A, vnl_vector < double > &b);
  std::pair < MATRIX*, VECTOR* > constructAb2(MRI *mriS, MRI *mriT);

  // conversions
  //MATRIX * rt2mat(MATRIX * r, MATRIX * t, MATRIX *outM); // uses global rtype flag
  //MATRIX * p2mat(MATRIX * p6, MATRIX *outM); // calls rt2mat (uses global rtype)

  bool needReslice(MRI *mri, double vsize = -1, int xdim =-1, int ydim=-1, int zdim=-1, bool fixtype = true);
  std::pair< MRI* , vnl_matrix_fixed < double, 4, 4> > makeIsotropic(MRI *mri, MRI *out, double vsize = -1, int xdim =-1, int ydim=-1, int zdim=-1, bool fixtype = true);

  void findSatMultiRes(const vnl_matrix < double > &mi, double scaleinit );

  // gaussian pyramid:
  std::pair < int, int > getGPLimits(MRI *mriS, MRI *mriT, int min, int max);
  std::vector < MRI* > buildGaussianPyramid (MRI * mri_in, int min = 16, int max = -1);
  std::vector < MRI* > buildGPLimits (MRI * mri_in,std::pair< int, int > limits);
  void freeGaussianPyramid(std::vector< MRI* >& p);
  void saveGaussianPyramid(std::vector< MRI* >& p, const std::string & prefix);

  MRI * mri_weights;
  MRI * mri_hweights;
  double wcheck; // set from computeRegistrationStepW
  double wchecksqrt; // set from computeRegistrationStepW
//  double zeroweights;// set from computeRegistrationStepW

  bool converged;

  // help vars
//	vnl_vector < double > lastp;

  MRI * mri_indexing;
};

template < class T > 
void Registration::iterativeRegistrationHelper( int nmax,double epsit, MRI * mriS, MRI* mriT, const vnl_matrix < double >& m, double scaleinit)
// helper is a template function to avoid code duplication
// computes iterative registration (recomputing A and b in each step)
// retruns 4x4 matrix Mfinal and iscalefinal (class member)
// The caller needs to retrieve any really final transform with getFinalVox2Vox
{
  if (!mriS) mriS = mri_source;
  if (!mriT) mriT = mri_target;

  assert (mriS && mriT);
  assert (nmax>0);

  std::pair < vnl_matrix_fixed<double , 4, 4>, double > cmd(vnl_matrix_fixed<double, 4, 4>(),0.0);
  std::pair < vnl_matrix_fixed<double , 4, 4>, double > fmd(vnl_matrix_fixed<double, 4, 4>(),0.0);

  // check if mi (inital transform) is passed
  if (!m.empty()) fmd.first = m;
  else if (!Minit.empty()) fmd.first = getMinitResampled();
  else fmd.first = initializeTransform(mriS,mriT);

  // ISCALECHANGE:
  // intensity model: R(s,IS,IT) = exp(-0.5 s) IT - exp(0.5 s) IS
  //                  R'  = -0.5 ( exp(-0.5 s) IT + exp(0.5 s) IS)
  //  thus iscalefinal= exp(0.5 s) * (1/exp(-0.5 s)) = exp(s)
  //     ==> s = log (iscalefinal)
  iscalefinal = 1.0;
  if (scaleinit != 1.0) iscalefinal = scaleinit;
	else iscalefinal = iscaleinit;
  fmd.second = log(iscalefinal);
  

  if (verbose > 1)
  {
    std::cout << "   - initial iscale: " << iscalefinal << std::endl;
    std::cout << "   - initial transform:\n" ;
		std::cout << fmd.first << std::endl;
  }

  //std::cout << "mris width " << mriS->width << std::endl;
  MRI* mri_Swarp   = NULL;
  MRI* mri_Twarp   = NULL;
  vnl_matrix_fixed<double , 4, 4> mh2;
  vnl_matrix_fixed<double , 4, 4> mh;
  vnl_matrix_fixed<double , 4, 4> mhi;mhi.set_identity();
//  vnl_matrix_fixed<double , 4, 4> mi;
	
  double diff = 100.0;
  double idiff = 0.0;
  double ieps = 0.001; // exp(ieps) = 1.0010005, so stop if below 0.1% change

  int i = 1;
	
  // here create RegistrationStep of the specific type: double or float:
  RegistrationStep<T> RStep(*this);
	RStep.setFloatSVD(true); // even with double, use float for SVD to save some memory
	T tt;	
		
  converged = false;
  while (!converged && i<=nmax  )
  {
    if (verbose >0) std::cout << " Iteration(" << typeid(tt).name() << "): " << i << std::endl;
		if (verbose == 1 && subsamplesize > 0)
    if (mriS->width > subsamplesize || mriS->height > subsamplesize || mriS->depth > subsamplesize)
      std::cout << " (subsample " << subsamplesize << ") " << std::flush;
		if (verbose >1) std::cout << std::endl;

    // map source (and if symmetric also target) to new space for next iteration
    // get mri_?warp and half way maps (mh, mhi):
    mapToNewSpace(fmd.first,iscalefinal,mriS,mriT,mri_Swarp,mri_Twarp,mh,mhi);
          
    if (debug > 0)
    {
      // write hw images before next registration step: 
      MRIwrite(mri_Swarp,(name+"-mriS-hw.mgz").c_str());
      MRIwrite(mri_Twarp,(name+"-mriT-hw.mgz").c_str());
      char ch;
      std::cout << "HW inputs written " << std::endl;
      std::cout << "Press a key to run registration: ";
      std::cin  >> ch;
    }

    // ==========================================================================
    //
    // compute Registration
    //
    if (verbose >1) std::cout << "   - compute new registration" << std::endl;
    cmd = RStep.computeRegistrationStep(mri_Swarp,mri_Twarp);
		wcheck = RStep.getwcheck();
		wchecksqrt = RStep.getwchecksqrt();
    // ==========================================================================

   // store M and d
    if (verbose >1) std::cout << "   - store transform" << std::endl;
		vnl_matrix_fixed < double, 4, 4 > fmdtmp(fmd.first);
    if (symmetry)
    {
      mh2 = vnl_inverse(mhi); // M = mh2 * mh
      // new M = mh2 * cm * mh
      fmd.first = (mh2 * cmd.first) * mh;
    }
    else fmd.first = cmd.first * fmd.first; // was '* mh' which should be the same

    // ISCALECHANGE:
    if (iscale)
    {
      fmd.second -= cmd.second; // adjust log
      iscalefinal = exp(fmd.second); // compute full factor (source to target)
      idiff = fabs(cmd.second);
      std::ostringstream istar;
      if (idiff <= ieps) istar << " <= " << ieps << "  :-)" ;
      if (verbose >0 ) std::cout << "     -- intensity log diff: abs(" << cmd.second << ") " << istar.str() << std::endl;
    }
		
    if (!rigid) diff = MyMatrix::getFrobeniusDiff(fmd.first, fmdtmp);
    else        diff = sqrt(MyMatrix::RigidTransDistSq(fmd.first, fmdtmp));
    if (verbose >1) std::cout << "     -- old diff. to prev. transform: " << diff << std::endl;
    diff = sqrt(MyMatrix::AffineTransDistSq(fmd.first, fmdtmp, 100));
		std::ostringstream star;
    if (diff <= epsit) star << "  <= " << epsit << "   :-)" ;
		else if (i == nmax) star<< " max it: " << nmax << " reached!";
    if (verbose >0) std::cout << "     -- diff. to prev. transform: " << diff << star.str() << std::endl;
    //std::cout << " intens: " << fmd.second << std::endl;
		i++;
    converged = (diff <= epsit && idiff <=ieps);
		
    if (debug > 0)
    {
      // write weights
      if (costfun == ROB)
      {
        if (mri_hweights) MRIfree(&mri_hweights);
 		    assert(RStep.getWeights());
 	      mri_hweights = MRIcopy(RStep.getWeights(),NULL);
        MRIwrite(mri_hweights,(name+"-mriS-hw-weights.mgz").c_str());
        char ch;
        std::cout << "Weights written for this iteration" << std::endl;
        std::cout << "Press a key to continue iterations: ";
        std::cin  >> ch;
      }
    } // end if debug
	
  } // end while loop
  
  // adjust converged (don't tell the outside about intensiy problems)
  converged = (diff <= epsit);
  
  if (mri_hweights) MRIfree(&mri_hweights);
  if (costfun == ROB)
  {
		assert(RStep.getWeights());
	  mri_hweights = MRIcopy(RStep.getWeights(),NULL);
	}

	
  //   DEBUG OUTPUT
  if (debug > 0)
  {
    // write weights and warped images after last step:

    MRIwrite(mri_Swarp,(name+"-mriS-mapped.mgz").c_str());
    MRIwrite(mri_Twarp,(name+"-mriT-mapped.mgz").c_str());
    MRI* salign = MRIclone(mriS,NULL);
    salign = MyMRI::MRIlinearTransform(mri_Swarp, salign,cmd.first);
    MRIwrite(salign,(name+"-mriS-align.mgz").c_str());
    MRIfree(&salign);
  } // end if debug

  // store weights (mapped to target space):
  if (mri_weights) MRIfree(&mri_weights);
  if (mri_hweights)
  {
    if (debug > 0)
    {
      // in the half-way space:
      std::string n = name+std::string("-mriS-weights.mgz");
      MRIwrite(mri_hweights,n.c_str());
    }
    // remove negative weights (markers) set to 1
    int x,y,z;
    for (z = 0 ; z < mri_hweights->depth  ; z++)
      for (x = 0 ; x < mri_hweights->width  ; x++)
        for (y = 0 ; y < mri_hweights->height ; y++)
        {
          if (MRIFvox(mri_hweights,x,y,z) < 0) MRIFvox(mri_hweights,x,y,z) = 1;
        }
				
    mri_weights = MRIalloc(mriT->width,mriT->height,mriT->depth,MRI_FLOAT);
    MRIcopyHeader(mriT,mri_weights);
    mri_weights->type = MRI_FLOAT;
    if (symmetry)
       mri_weights = MyMRI::MRIlinearTransform(mri_hweights,mri_weights,mh2);
    else
       mri_weights = MRIcopy(mri_hweights,mri_weights);
  }

  //vnl_matlab_print(vcl_cerr,fmd.first,"fmd",vnl_matlab_print_format_long);std::cerr << std::endl;
  //vnl_matlab_print(vcl_cerr,cmd.first,"cmd",vnl_matlab_print_format_long);std::cerr << std::endl;
  //vnl_matlab_print(vcl_cerr,mh,"mov1hw",vnl_matlab_print_format_long);std::cerr << std::endl;
  //vnl_matlab_print(vcl_cerr,mhi,"dst1hw",vnl_matlab_print_format_long);std::cerr << std::endl;

  // adjust half way maps to new midpoint based on final transform
  if (verbose >1) std::cout << "     -- adjusting half-way maps " << std::endl;
  vnl_matrix_fixed < double, 4, 4 > ch = MyMatrix::MatrixSqrt(fmd.first);
  // do not just assume c = ch*ch, rather c = ch2 * ch
  // for transforming target we need ch2^-1 = ch * c^-1
  vnl_matrix_fixed < double, 4, 4 >  ci  = vnl_inverse(fmd.first);
  vnl_matrix_fixed < double, 4, 4 >  chi = ch*ci;
  mov2weights = ch; 
  dst2weights = chi;

  //vnl_matlab_print(vcl_cerr,mov2weights,"mov2hw",vnl_matlab_print_format_long);std::cerr << std::endl;
  //vnl_matlab_print(vcl_cerr,dst2weights,"dst2hw",vnl_matlab_print_format_long);std::cerr << std::endl;


  MRIfree(&mri_Twarp);
  MRIfree(&mri_Swarp);

  Mfinal = fmd.first;

}



#endif
