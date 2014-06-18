/*
     SVD routines for setting solver options.

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2013, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.

   SLEPc is free software: you can redistribute it and/or modify it under  the
   terms of version 3 of the GNU Lesser General Public License as published by
   the Free Software Foundation.

   SLEPc  is  distributed in the hope that it will be useful, but WITHOUT  ANY
   WARRANTY;  without even the implied warranty of MERCHANTABILITY or  FITNESS
   FOR  A  PARTICULAR PURPOSE. See the GNU Lesser General Public  License  for
   more details.

   You  should have received a copy of the GNU Lesser General  Public  License
   along with SLEPc. If not, see <http://www.gnu.org/licenses/>.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include <slepc-private/svdimpl.h>      /*I "slepcsvd.h" I*/

#undef __FUNCT__
#define __FUNCT__ "SVDSetTransposeMode"
/*@
   SVDSetTransposeMode - Sets how to handle the transpose of the matrix
   associated with the singular value problem.

   Logically Collective on SVD

   Input Parameters:
+  svd  - the singular value solver context
-  mode - how to compute the transpose, one of SVD_TRANSPOSE_EXPLICIT
          or SVD_TRANSPOSE_IMPLICIT (see notes below)

   Options Database Key:
.  -svd_transpose_mode <mode> - Indicates the mode flag, where <mode>
    is one of 'explicit' or 'implicit'.

   Notes:
   In the SVD_TRANSPOSE_EXPLICIT mode, the transpose of the matrix is
   explicitly built.

   The option SVD_TRANSPOSE_IMPLICIT does not build the transpose, but
   handles it implicitly via MatMultTranspose() (or MatMultHermitianTranspose()
   in the complex case) operations. This is
   likely to be more inefficient than SVD_TRANSPOSE_EXPLICIT, both in
   sequential and in parallel, but requires less storage.

   The default is SVD_TRANSPOSE_EXPLICIT if the matrix has defined the
   MatTranspose operation, and SVD_TRANSPOSE_IMPLICIT otherwise.

   Level: advanced

.seealso: SVDGetTransposeMode(), SVDSolve(), SVDSetOperator(),
   SVDGetOperator(), SVDTransposeMode
@*/
PetscErrorCode SVDSetTransposeMode(SVD svd,SVDTransposeMode mode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveEnum(svd,mode,2);
  if (mode == PETSC_DEFAULT || mode == PETSC_DECIDE) mode = (SVDTransposeMode)PETSC_DECIDE;
  else switch (mode) {
    case SVD_TRANSPOSE_EXPLICIT:
    case SVD_TRANSPOSE_IMPLICIT:
      if (svd->transmode!=mode) {
        svd->transmode = mode;
        svd->setupcalled = 0;
      }
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Invalid transpose mode");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetTransposeMode"
/*@C
   SVDGetTransposeMode - Gets the mode used to compute the transpose
   of the matrix associated with the singular value problem.

   Not Collective

   Input Parameter:
.  svd  - the singular value solver context

   Output paramter:
.  mode - how to compute the transpose, one of SVD_TRANSPOSE_EXPLICIT
          or SVD_TRANSPOSE_IMPLICIT

   Level: advanced

.seealso: SVDSetTransposeMode(), SVDSolve(), SVDSetOperator(),
   SVDGetOperator(), SVDTransposeMode
@*/
PetscErrorCode SVDGetTransposeMode(SVD svd,SVDTransposeMode *mode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidPointer(mode,2);
  *mode = svd->transmode;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDSetTolerances"
/*@
   SVDSetTolerances - Sets the tolerance and maximum
   iteration count used by the default SVD convergence testers.

   Logically Collective on SVD

   Input Parameters:
+  svd - the singular value solver context
.  tol - the convergence tolerance
-  maxits - maximum number of iterations to use

   Options Database Keys:
+  -svd_tol <tol> - Sets the convergence tolerance
-  -svd_max_it <maxits> - Sets the maximum number of iterations allowed
   (use PETSC_DECIDE to compute an educated guess based on basis and matrix sizes)

   Notes:
   Pass 0 to retain the previous value of any parameter.

   Level: intermediate

.seealso: SVDGetTolerances()
@*/
PetscErrorCode SVDSetTolerances(SVD svd,PetscReal tol,PetscInt maxits)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveReal(svd,tol,2);
  PetscValidLogicalCollectiveInt(svd,maxits,3);
  if (tol) {
    if (tol == PETSC_DEFAULT) {
      tol = PETSC_DEFAULT;
    } else {
      if (tol < 0.0) SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of tol. Must be > 0");
      svd->tol = tol;
    }
  }
  if (maxits) {
    if (maxits == PETSC_DEFAULT || maxits == PETSC_DECIDE) {
      svd->max_it = 0;
      svd->setupcalled = 0;
    } else {
      if (maxits < 0) SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of maxits. Must be > 0");
      svd->max_it = maxits;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetTolerances"
/*@
   SVDGetTolerances - Gets the tolerance and maximum
   iteration count used by the default SVD convergence tests.

   Not Collective

   Input Parameter:
.  svd - the singular value solver context

   Output Parameters:
+  tol - the convergence tolerance
-  maxits - maximum number of iterations

   Notes:
   The user can specify NULL for any parameter that is not needed.

   Level: intermediate

.seealso: SVDSetTolerances()
@*/
PetscErrorCode SVDGetTolerances(SVD svd,PetscReal *tol,PetscInt *maxits)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (tol)    *tol    = svd->tol;
  if (maxits) *maxits = svd->max_it;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDSetDimensions"
/*@
   SVDSetDimensions - Sets the number of singular values to compute
   and the dimension of the subspace.

   Logically Collective on SVD

   Input Parameters:
+  svd - the singular value solver context
.  nsv - number of singular values to compute
.  ncv - the maximum dimension of the subspace to be used by the solver
-  mpd - the maximum dimension allowed for the projected problem

   Options Database Keys:
+  -svd_nsv <nsv> - Sets the number of singular values
.  -svd_ncv <ncv> - Sets the dimension of the subspace
-  -svd_mpd <mpd> - Sets the maximum projected dimension

   Notes:
   Pass 0 to retain the previous value of any parameter.

   Use PETSC_DECIDE for ncv and mpd to assign a reasonably good value, which is
   dependent on the solution method and the number of singular values required.

   The parameters ncv and mpd are intimately related, so that the user is advised
   to set one of them at most. Normal usage is that
   (a) in cases where nsv is small, the user sets ncv (a reasonable default is 2*nsv); and
   (b) in cases where nsv is large, the user sets mpd.

   The value of ncv should always be between nsv and (nsv+mpd), typically
   ncv=nsv+mpd. If nev is not too large, mpd=nsv is a reasonable choice, otherwise
   a smaller value should be used.

   Level: intermediate

.seealso: SVDGetDimensions()
@*/
PetscErrorCode SVDSetDimensions(SVD svd,PetscInt nsv,PetscInt ncv,PetscInt mpd)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveInt(svd,nsv,2);
  PetscValidLogicalCollectiveInt(svd,ncv,3);
  PetscValidLogicalCollectiveInt(svd,mpd,4);
  if (nsv) {
    if (nsv<1) SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of nsv. Must be > 0");
    svd->nsv = nsv;
  }
  if (ncv) {
    if (ncv == PETSC_DEFAULT || ncv == PETSC_DECIDE) {
      svd->ncv = 0;
    } else {
      if (ncv<1) SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of ncv. Must be > 0");
      svd->ncv = ncv;
    }
    svd->setupcalled = 0;
  }
  if (mpd) {
    if (mpd == PETSC_DECIDE || mpd == PETSC_DEFAULT) {
      svd->mpd = 0;
    } else {
      if (mpd<1) SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Illegal value of mpd. Must be > 0");
      svd->mpd = mpd;
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetDimensions"
/*@
   SVDGetDimensions - Gets the number of singular values to compute
   and the dimension of the subspace.

   Not Collective

   Input Parameter:
.  svd - the singular value context

   Output Parameters:
+  nsv - number of singular values to compute
.  ncv - the maximum dimension of the subspace to be used by the solver
-  mpd - the maximum dimension allowed for the projected problem

   Notes:
   The user can specify NULL for any parameter that is not needed.

   Level: intermediate

.seealso: SVDSetDimensions()
@*/
PetscErrorCode SVDGetDimensions(SVD svd,PetscInt *nsv,PetscInt *ncv,PetscInt *mpd)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (nsv) *nsv = svd->nsv;
  if (ncv) *ncv = svd->ncv;
  if (mpd) *mpd = svd->mpd;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDSetWhichSingularTriplets"
/*@
    SVDSetWhichSingularTriplets - Specifies which singular triplets are
    to be sought.

    Logically Collective on SVD

    Input Parameter:
.   svd - singular value solver context obtained from SVDCreate()

    Output Parameter:
.   which - which singular triplets are to be sought

    Possible values:
    The parameter 'which' can have one of these values:

+     SVD_LARGEST  - largest singular values
-     SVD_SMALLEST - smallest singular values

    Options Database Keys:
+   -svd_largest  - Sets largest singular values
-   -svd_smallest - Sets smallest singular values

    Level: intermediate

.seealso: SVDGetWhichSingularTriplets(), SVDWhich
@*/
PetscErrorCode SVDSetWhichSingularTriplets(SVD svd,SVDWhich which)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveEnum(svd,which,2);
  switch (which) {
    case SVD_LARGEST:
    case SVD_SMALLEST:
      if (svd->which != which) {
        svd->setupcalled = 0;
        svd->which = which;
      }
      break;
  default:
    SETERRQ(PetscObjectComm((PetscObject)svd),PETSC_ERR_ARG_OUTOFRANGE,"Invalid 'which' parameter");
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetWhichSingularTriplets"
/*@C
    SVDGetWhichSingularTriplets - Returns which singular triplets are
    to be sought.

    Not Collective

    Input Parameter:
.   svd - singular value solver context obtained from SVDCreate()

    Output Parameter:
.   which - which singular triplets are to be sought

    Notes:
    See SVDSetWhichSingularTriplets() for possible values of which

    Level: intermediate

.seealso: SVDSetWhichSingularTriplets(), SVDWhich
@*/
PetscErrorCode SVDGetWhichSingularTriplets(SVD svd,SVDWhich *which)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidPointer(which,2);
  *which = svd->which;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDSetFromOptions"
/*@
   SVDSetFromOptions - Sets SVD options from the options database.
   This routine must be called before SVDSetUp() if the user is to be
   allowed to set the solver type.

   Collective on SVD

   Input Parameters:
.  svd - the singular value solver context

   Notes:
   To see all options, run your program with the -help option.

   Level: beginner

.seealso:
@*/
PetscErrorCode SVDSetFromOptions(SVD svd)
{
  PetscErrorCode   ierr;
  char             type[256],monfilename[PETSC_MAX_PATH_LEN];
  PetscBool        flg;
  const char       *mode_list[2] = {"explicit","implicit"};
  PetscInt         i,j,k;
  PetscReal        r;
  PetscViewer      monviewer;
  SlepcConvMonitor ctx;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  svd->setupcalled = 0;
  if (!SVDRegisterAllCalled) { ierr = SVDRegisterAll();CHKERRQ(ierr); }
  ierr = PetscObjectOptionsBegin((PetscObject)svd);CHKERRQ(ierr);

  ierr = PetscOptionsFList("-svd_type","Singular Value Solver method","SVDSetType",SVDList,(char*)(((PetscObject)svd)->type_name?((PetscObject)svd)->type_name:SVDCROSS),type,256,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = SVDSetType(svd,type);CHKERRQ(ierr);
  } else if (!((PetscObject)svd)->type_name) {
    ierr = SVDSetType(svd,SVDCROSS);CHKERRQ(ierr);
  }

  ierr = PetscOptionsName("-svd_view","Print detailed information on solver used","SVDView",&flg);CHKERRQ(ierr);

  ierr = PetscOptionsEList("-svd_transpose_mode","Transpose SVD mode","SVDSetTransposeMode",mode_list,2,svd->transmode == PETSC_DECIDE ? "decide" : mode_list[svd->transmode],&i,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = SVDSetTransposeMode(svd,(SVDTransposeMode)i);CHKERRQ(ierr);
  }

  r = i = 0;
  ierr = PetscOptionsInt("-svd_max_it","Maximum number of iterations","SVDSetTolerances",svd->max_it,&i,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-svd_tol","Tolerance","SVDSetTolerances",svd->tol==PETSC_DEFAULT?SLEPC_DEFAULT_TOL:svd->tol,&r,NULL);CHKERRQ(ierr);
  ierr = SVDSetTolerances(svd,r,i);CHKERRQ(ierr);

  i = j = k = 0;
  ierr = PetscOptionsInt("-svd_nsv","Number of singular values to compute","SVDSetDimensions",svd->nsv,&i,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-svd_ncv","Number of basis vectors","SVDSetDimensions",svd->ncv,&j,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsInt("-svd_mpd","Maximum dimension of projected problem","SVDSetDimensions",svd->mpd,&k,NULL);CHKERRQ(ierr);
  ierr = SVDSetDimensions(svd,i,j,k);CHKERRQ(ierr);

  ierr = PetscOptionsBoolGroupBegin("-svd_largest","compute largest singular values","SVDSetWhichSingularTriplets",&flg);CHKERRQ(ierr);
  if (flg) { ierr = SVDSetWhichSingularTriplets(svd,SVD_LARGEST);CHKERRQ(ierr); }
  ierr = PetscOptionsBoolGroupEnd("-svd_smallest","compute smallest singular values","SVDSetWhichSingularTriplets",&flg);CHKERRQ(ierr);
  if (flg) { ierr = SVDSetWhichSingularTriplets(svd,SVD_SMALLEST);CHKERRQ(ierr); }

  flg = PETSC_FALSE;
  ierr = PetscOptionsBool("-svd_monitor_cancel","Remove any hardwired monitor routines","SVDMonitorCancel",flg,&flg,NULL);CHKERRQ(ierr);
  if (flg) {
    ierr = SVDMonitorCancel(svd);CHKERRQ(ierr);
  }

  ierr = PetscOptionsString("-svd_monitor_all","Monitor approximate singular values and error estimates","SVDMonitorSet","stdout",monfilename,PETSC_MAX_PATH_LEN,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscViewerASCIIOpen(PetscObjectComm((PetscObject)svd),monfilename,&monviewer);CHKERRQ(ierr);
    ierr = SVDMonitorSet(svd,SVDMonitorAll,monviewer,(PetscErrorCode (*)(void**))PetscViewerDestroy);CHKERRQ(ierr);
    ierr = SVDSetTrackAll(svd,PETSC_TRUE);CHKERRQ(ierr);
  }
  ierr = PetscOptionsString("-svd_monitor_conv","Monitor approximate singular values and error estimates as they converge","SVDMonitorSet","stdout",monfilename,PETSC_MAX_PATH_LEN,&flg);CHKERRQ(ierr);
  if (flg) {
      ierr = PetscNew(&ctx);CHKERRQ(ierr);
      ierr = PetscViewerASCIIOpen(PetscObjectComm((PetscObject)svd),monfilename,&ctx->viewer);CHKERRQ(ierr);
      ierr = SVDMonitorSet(svd,SVDMonitorConverged,ctx,(PetscErrorCode (*)(void**))SlepcConvMonitorDestroy);CHKERRQ(ierr);
  }
  ierr = PetscOptionsString("-svd_monitor","Monitor first unconverged approximate singular value and error estimate","SVDMonitorSet","stdout",monfilename,PETSC_MAX_PATH_LEN,&flg);CHKERRQ(ierr);
  if (flg) {
    ierr = PetscViewerASCIIOpen(PetscObjectComm((PetscObject)svd),monfilename,&monviewer);CHKERRQ(ierr);
    ierr = SVDMonitorSet(svd,SVDMonitorFirst,monviewer,(PetscErrorCode (*)(void**))PetscViewerDestroy);CHKERRQ(ierr);
  }
  flg = PETSC_FALSE;
  ierr = PetscOptionsBool("-svd_monitor_lg","Monitor first unconverged approximate singular value and error estimate graphically","SVDMonitorSet",flg,&flg,NULL);CHKERRQ(ierr);
  if (flg) {
    ierr = SVDMonitorSet(svd,SVDMonitorLG,NULL,NULL);CHKERRQ(ierr);
  }
  flg = PETSC_FALSE;
  ierr = PetscOptionsBool("-svd_monitor_lg_all","Monitor error estimates graphically","SVDMonitorSet",flg,&flg,NULL);CHKERRQ(ierr);
  if (flg) {
    ierr = SVDMonitorSet(svd,SVDMonitorLGAll,NULL,NULL);CHKERRQ(ierr);
    ierr = SVDSetTrackAll(svd,PETSC_TRUE);CHKERRQ(ierr);
  }
  if (svd->ops->setfromoptions) {
    ierr = (*svd->ops->setfromoptions)(svd);CHKERRQ(ierr);
  }

  ierr = PetscObjectProcessOptionsHandlers((PetscObject)svd);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  if (!svd->V) { ierr = SVDGetBV(svd,&svd->V,&svd->U);CHKERRQ(ierr); }
  ierr = BVSetFromOptions(svd->V);CHKERRQ(ierr);
  ierr = BVSetFromOptions(svd->U);CHKERRQ(ierr);
  if (!svd->ds) { ierr = SVDGetDS(svd,&svd->ds);CHKERRQ(ierr); }
  ierr = DSSetFromOptions(svd->ds);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(svd->rand);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDSetTrackAll"
/*@
   SVDSetTrackAll - Specifies if the solver must compute the residual norm of all
   approximate singular value or not.

   Logically Collective on SVD

   Input Parameters:
+  svd      - the singular value solver context
-  trackall - whether to compute all residuals or not

   Notes:
   If the user sets trackall=PETSC_TRUE then the solver computes (or estimates)
   the residual norm for each singular value approximation. Computing the residual is
   usually an expensive operation and solvers commonly compute only the residual
   associated to the first unconverged singular value.

   The options '-svd_monitor_all' and '-svd_monitor_lg_all' automatically
   activate this option.

   Level: intermediate

.seealso: SVDGetTrackAll()
@*/
PetscErrorCode SVDSetTrackAll(SVD svd,PetscBool trackall)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidLogicalCollectiveBool(svd,trackall,2);
  svd->trackall = trackall;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetTrackAll"
/*@
   SVDGetTrackAll - Returns the flag indicating whether all residual norms must
   be computed or not.

   Not Collective

   Input Parameter:
.  svd - the singular value solver context

   Output Parameter:
.  trackall - the returned flag

   Level: intermediate

.seealso: SVDSetTrackAll()
@*/
PetscErrorCode SVDGetTrackAll(SVD svd,PetscBool *trackall)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidPointer(trackall,2);
  *trackall = svd->trackall;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "SVDSetOptionsPrefix"
/*@C
   SVDSetOptionsPrefix - Sets the prefix used for searching for all
   SVD options in the database.

   Logically Collective on SVD

   Input Parameters:
+  svd - the singular value solver context
-  prefix - the prefix string to prepend to all SVD option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   For example, to distinguish between the runtime options for two
   different SVD contexts, one could call
.vb
      SVDSetOptionsPrefix(svd1,"svd1_")
      SVDSetOptionsPrefix(svd2,"svd2_")
.ve

   Level: advanced

.seealso: SVDAppendOptionsPrefix(), SVDGetOptionsPrefix()
@*/
PetscErrorCode SVDSetOptionsPrefix(SVD svd,const char *prefix)
{
  PetscErrorCode ierr;
  PetscBool      flg1,flg2;
  EPS            eps;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (!svd->V) { ierr = SVDGetBV(svd,&svd->V,&svd->U);CHKERRQ(ierr); }
  ierr = BVSetOptionsPrefix(svd->V,prefix);CHKERRQ(ierr);
  ierr = BVSetOptionsPrefix(svd->U,prefix);CHKERRQ(ierr);
  if (!svd->ds) { ierr = SVDGetDS(svd,&svd->ds);CHKERRQ(ierr); }
  ierr = DSSetOptionsPrefix(svd->ds,prefix);CHKERRQ(ierr);
  ierr = PetscObjectSetOptionsPrefix((PetscObject)svd,prefix);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)svd,SVDCROSS,&flg1);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)svd,SVDCYCLIC,&flg2);CHKERRQ(ierr);
  if (flg1) {
    ierr = SVDCrossGetEPS(svd,&eps);CHKERRQ(ierr);
  } else if (flg2) {
      ierr = SVDCyclicGetEPS(svd,&eps);CHKERRQ(ierr);
  }
  if (flg1 || flg2) {
    ierr = EPSSetOptionsPrefix(eps,prefix);CHKERRQ(ierr);
    ierr = EPSAppendOptionsPrefix(eps,"svd_");CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDAppendOptionsPrefix"
/*@C
   SVDAppendOptionsPrefix - Appends to the prefix used for searching for all
   SVD options in the database.

   Logically Collective on SVD

   Input Parameters:
+  svd - the singular value solver context
-  prefix - the prefix string to prepend to all SVD option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

   Level: advanced

.seealso: SVDSetOptionsPrefix(), SVDGetOptionsPrefix()
@*/
PetscErrorCode SVDAppendOptionsPrefix(SVD svd,const char *prefix)
{
  PetscErrorCode ierr;
  PetscBool      flg1,flg2;
  EPS            eps;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  if (!svd->V) { ierr = SVDGetBV(svd,&svd->V,&svd->U);CHKERRQ(ierr); }
  ierr = BVSetOptionsPrefix(svd->V,prefix);CHKERRQ(ierr);
  ierr = BVSetOptionsPrefix(svd->U,prefix);CHKERRQ(ierr);
  if (!svd->ds) { ierr = SVDGetDS(svd,&svd->ds);CHKERRQ(ierr); }
  ierr = DSSetOptionsPrefix(svd->ds,prefix);CHKERRQ(ierr);
  ierr = PetscObjectAppendOptionsPrefix((PetscObject)svd,prefix);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)svd,SVDCROSS,&flg1);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)svd,SVDCYCLIC,&flg2);CHKERRQ(ierr);
  if (flg1) {
    ierr = SVDCrossGetEPS(svd,&eps);CHKERRQ(ierr);
  } else if (flg2) {
    ierr = SVDCyclicGetEPS(svd,&eps);CHKERRQ(ierr);
  }
  if (flg1 || flg2) {
    ierr = EPSSetOptionsPrefix(eps,((PetscObject)svd)->prefix);CHKERRQ(ierr);
    ierr = EPSAppendOptionsPrefix(eps,"svd_");CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "SVDGetOptionsPrefix"
/*@C
   SVDGetOptionsPrefix - Gets the prefix used for searching for all
   SVD options in the database.

   Not Collective

   Input Parameters:
.  svd - the singular value solver context

   Output Parameters:
.  prefix - pointer to the prefix string used is returned

   Notes: On the fortran side, the user should pass in a string 'prefix' of
   sufficient length to hold the prefix.

   Level: advanced

.seealso: SVDSetOptionsPrefix(), SVDAppendOptionsPrefix()
@*/
PetscErrorCode SVDGetOptionsPrefix(SVD svd,const char *prefix[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(svd,SVD_CLASSID,1);
  PetscValidPointer(prefix,2);
  ierr = PetscObjectGetOptionsPrefix((PetscObject)svd,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

