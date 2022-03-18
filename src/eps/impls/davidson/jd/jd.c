/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   SLEPc eigensolver: "jd"

   Method: Jacobi-Davidson

   Algorithm:

       Jacobi-Davidson with various subspace extraction and
       restart techniques.

   References:

       [1] G.L.G. Sleijpen and H.A. van der Vorst, "A Jacobi-Davidson
           iteration method for linear eigenvalue problems", SIAM J.
           Matrix Anal. Appl. 17(2):401-425, 1996.

       [2] E. Romero and J.E. Roman, "A parallel implementation of
           Davidson methods for large-scale eigenvalue problems in
           SLEPc", ACM Trans. Math. Software 40(2), Article 13, 2014.
*/

#include <slepc/private/epsimpl.h>                /*I "slepceps.h" I*/
#include <../src/eps/impls/davidson/davidson.h>

PetscErrorCode EPSSetFromOptions_JD(PetscOptionItems *PetscOptionsObject,EPS eps)
{
  PetscBool      flg,flg2,op,orth;
  PetscInt       opi,opi0;
  PetscReal      opf;

  PetscFunctionBegin;
  CHKERRQ(PetscOptionsHead(PetscOptionsObject,"EPS Jacobi-Davidson (JD) Options"));

    CHKERRQ(EPSJDGetKrylovStart(eps,&op));
    CHKERRQ(PetscOptionsBool("-eps_jd_krylov_start","Start the search subspace with a Krylov basis","EPSJDSetKrylovStart",op,&op,&flg));
    if (flg) CHKERRQ(EPSJDSetKrylovStart(eps,op));

    CHKERRQ(EPSJDGetBOrth(eps,&orth));
    CHKERRQ(PetscOptionsBool("-eps_jd_borth","Use B-orthogonalization in the search subspace","EPSJDSetBOrth",op,&op,&flg));
    if (flg) CHKERRQ(EPSJDSetBOrth(eps,op));

    CHKERRQ(EPSJDGetBlockSize(eps,&opi));
    CHKERRQ(PetscOptionsInt("-eps_jd_blocksize","Number of vectors to add to the search subspace","EPSJDSetBlockSize",opi,&opi,&flg));
    if (flg) CHKERRQ(EPSJDSetBlockSize(eps,opi));

    CHKERRQ(EPSJDGetRestart(eps,&opi,&opi0));
    CHKERRQ(PetscOptionsInt("-eps_jd_minv","Size of the search subspace after restarting","EPSJDSetRestart",opi,&opi,&flg));
    CHKERRQ(PetscOptionsInt("-eps_jd_plusk","Number of eigenvectors saved from the previous iteration when restarting","EPSJDSetRestart",opi0,&opi0,&flg2));
    if (flg || flg2) CHKERRQ(EPSJDSetRestart(eps,opi,opi0));

    CHKERRQ(EPSJDGetInitialSize(eps,&opi));
    CHKERRQ(PetscOptionsInt("-eps_jd_initial_size","Initial size of the search subspace","EPSJDSetInitialSize",opi,&opi,&flg));
    if (flg) CHKERRQ(EPSJDSetInitialSize(eps,opi));

    CHKERRQ(EPSJDGetFix(eps,&opf));
    CHKERRQ(PetscOptionsReal("-eps_jd_fix","Tolerance for changing the target in the correction equation","EPSJDSetFix",opf,&opf,&flg));
    if (flg) CHKERRQ(EPSJDSetFix(eps,opf));

    CHKERRQ(EPSJDGetConstCorrectionTol(eps,&op));
    CHKERRQ(PetscOptionsBool("-eps_jd_const_correction_tol","Disable the dynamic stopping criterion when solving the correction equation","EPSJDSetConstCorrectionTol",op,&op,&flg));
    if (flg) CHKERRQ(EPSJDSetConstCorrectionTol(eps,op));

  CHKERRQ(PetscOptionsTail());
  PetscFunctionReturn(0);
}

PetscErrorCode EPSSetDefaultST_JD(EPS eps)
{
  KSP            ksp;

  PetscFunctionBegin;
  if (!((PetscObject)eps->st)->type_name) {
    CHKERRQ(STSetType(eps->st,STPRECOND));
    CHKERRQ(STPrecondSetKSPHasMat(eps->st,PETSC_TRUE));
  }
  CHKERRQ(STGetKSP(eps->st,&ksp));
  if (!((PetscObject)ksp)->type_name) {
    CHKERRQ(KSPSetType(ksp,KSPBCGSL));
    CHKERRQ(KSPSetTolerances(ksp,1e-4,PETSC_DEFAULT,PETSC_DEFAULT,90));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode EPSSetUp_JD(EPS eps)
{
  PetscBool      t;
  KSP            ksp;

  PetscFunctionBegin;
  /* Setup common for all davidson solvers */
  CHKERRQ(EPSSetUp_XD(eps));

  /* Check some constraints */
  CHKERRQ(STGetKSP(eps->st,&ksp));
  CHKERRQ(PetscObjectTypeCompare((PetscObject)ksp,KSPPREONLY,&t));
  PetscCheck(!t,PetscObjectComm((PetscObject)eps),PETSC_ERR_SUP,"EPSJD does not work with KSPPREONLY");
  PetscFunctionReturn(0);
}

PetscErrorCode EPSView_JD(EPS eps,PetscViewer viewer)
{
  PetscBool      isascii,opb;
  PetscReal      opf;
  PetscInt       opi,opi0;

  PetscFunctionBegin;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii));
  if (isascii) {
    CHKERRQ(EPSXDGetBOrth_XD(eps,&opb));
    if (opb) {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  search subspace is B-orthogonalized\n"));
    } else {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  search subspace is orthogonalized\n"));
    }
    CHKERRQ(EPSXDGetBlockSize_XD(eps,&opi));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  block size=%" PetscInt_FMT "\n",opi));
    CHKERRQ(EPSXDGetKrylovStart_XD(eps,&opb));
    if (!opb) {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  type of the initial subspace: non-Krylov\n"));
    } else {
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"  type of the initial subspace: Krylov\n"));
    }
    CHKERRQ(EPSXDGetRestart_XD(eps,&opi,&opi0));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  size of the subspace after restarting: %" PetscInt_FMT "\n",opi));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  number of vectors after restarting from the previous iteration: %" PetscInt_FMT "\n",opi0));

    CHKERRQ(EPSJDGetFix_JD(eps,&opf));
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  threshold for changing the target in the correction equation (fix): %g\n",(double)opf));

    CHKERRQ(EPSJDGetConstCorrectionTol_JD(eps,&opb));
    if (!opb) CHKERRQ(PetscViewerASCIIPrintf(viewer,"  using dynamic tolerance for the correction equation\n"));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode EPSDestroy_JD(EPS eps)
{
  PetscFunctionBegin;
  CHKERRQ(PetscFree(eps->data));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetKrylovStart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetKrylovStart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetBlockSize_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetBlockSize_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetRestart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetRestart_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetInitialSize_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetInitialSize_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetFix_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetFix_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetConstCorrectionTol_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetConstCorrectionTol_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetBOrth_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetBOrth_C",NULL));
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetKrylovStart - Activates or deactivates starting the searching
   subspace with a Krylov basis.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
-  krylovstart - boolean flag

   Options Database Key:
.  -eps_jd_krylov_start - Activates starting the searching subspace with a
    Krylov basis

   Level: advanced

.seealso: EPSJDGetKrylovStart()
@*/
PetscErrorCode EPSJDSetKrylovStart(EPS eps,PetscBool krylovstart)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveBool(eps,krylovstart,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetKrylovStart_C",(EPS,PetscBool),(eps,krylovstart)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetKrylovStart - Returns a flag indicating if the searching subspace is started with a
   Krylov basis.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameters:
.  krylovstart - boolean flag indicating if the searching subspace is started
   with a Krylov basis

   Level: advanced

.seealso: EPSJDSetKrylovStart()
@*/
PetscErrorCode EPSJDGetKrylovStart(EPS eps,PetscBool *krylovstart)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidBoolPointer(krylovstart,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetKrylovStart_C",(EPS,PetscBool*),(eps,krylovstart)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetBlockSize - Sets the number of vectors to be added to the searching space
   in every iteration.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
-  blocksize - number of vectors added to the search space in every iteration

   Options Database Key:
.  -eps_jd_blocksize - number of vectors added to the searching space every iteration

   Level: advanced

.seealso: EPSJDSetKrylovStart()
@*/
PetscErrorCode EPSJDSetBlockSize(EPS eps,PetscInt blocksize)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveInt(eps,blocksize,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetBlockSize_C",(EPS,PetscInt),(eps,blocksize)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetBlockSize - Returns the number of vectors to be added to the searching space
   in every iteration.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameter:
.  blocksize - number of vectors added to the search space in every iteration

   Level: advanced

.seealso: EPSJDSetBlockSize()
@*/
PetscErrorCode EPSJDGetBlockSize(EPS eps,PetscInt *blocksize)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidIntPointer(blocksize,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetBlockSize_C",(EPS,PetscInt*),(eps,blocksize)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetRestart - Sets the number of vectors of the searching space after
   restarting and the number of vectors saved from the previous iteration.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
.  minv - number of vectors of the searching subspace after restarting
-  plusk - number of vectors saved from the previous iteration

   Options Database Keys:
+  -eps_jd_minv - number of vectors of the searching subspace after restarting
-  -eps_jd_plusk - number of vectors saved from the previous iteration

   Level: advanced

.seealso: EPSJDGetRestart()
@*/
PetscErrorCode EPSJDSetRestart(EPS eps,PetscInt minv,PetscInt plusk)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveInt(eps,minv,2);
  PetscValidLogicalCollectiveInt(eps,plusk,3);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetRestart_C",(EPS,PetscInt,PetscInt),(eps,minv,plusk)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetRestart - Gets the number of vectors of the searching space after
   restarting and the number of vectors saved from the previous iteration.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameters:
+  minv - number of vectors of the searching subspace after restarting
-  plusk - number of vectors saved from the previous iteration

   Level: advanced

.seealso: EPSJDSetRestart()
@*/
PetscErrorCode EPSJDGetRestart(EPS eps,PetscInt *minv,PetscInt *plusk)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetRestart_C",(EPS,PetscInt*,PetscInt*),(eps,minv,plusk)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetInitialSize - Sets the initial size of the searching space.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
-  initialsize - number of vectors of the initial searching subspace

   Options Database Key:
.  -eps_jd_initial_size - number of vectors of the initial searching subspace

   Notes:
   If EPSJDGetKrylovStart() is PETSC_FALSE and the user provides vectors with
   EPSSetInitialSpace(), up to initialsize vectors will be used; and if the
   provided vectors are not enough, the solver completes the subspace with
   random vectors. In the case of EPSJDGetKrylovStart() being PETSC_TRUE, the solver
   gets the first vector provided by the user or, if not available, a random vector,
   and expands the Krylov basis up to initialsize vectors.

   Level: advanced

.seealso: EPSJDGetInitialSize(), EPSJDGetKrylovStart()
@*/
PetscErrorCode EPSJDSetInitialSize(EPS eps,PetscInt initialsize)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveInt(eps,initialsize,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetInitialSize_C",(EPS,PetscInt),(eps,initialsize)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetInitialSize - Returns the initial size of the searching space.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameter:
.  initialsize - number of vectors of the initial searching subspace

   Notes:
   If EPSJDGetKrylovStart() is PETSC_FALSE and the user provides vectors with
   EPSSetInitialSpace(), up to initialsize vectors will be used; and if the
   provided vectors are not enough, the solver completes the subspace with
   random vectors. In the case of EPSJDGetKrylovStart() being PETSC_TRUE, the solver
   gets the first vector provided by the user or, if not available, a random vector,
   and expands the Krylov basis up to initialsize vectors.

   Level: advanced

.seealso: EPSJDSetInitialSize(), EPSJDGetKrylovStart()
@*/
PetscErrorCode EPSJDGetInitialSize(EPS eps,PetscInt *initialsize)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidIntPointer(initialsize,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetInitialSize_C",(EPS,PetscInt*),(eps,initialsize)));
  PetscFunctionReturn(0);
}

PetscErrorCode EPSJDSetFix_JD(EPS eps,PetscReal fix)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  if (fix == PETSC_DEFAULT || fix == PETSC_DECIDE) fix = 0.01;
  PetscCheck(fix>0.0,PetscObjectComm((PetscObject)eps),PETSC_ERR_ARG_OUTOFRANGE,"Invalid fix value, must be >0");
  data->fix = fix;
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetFix - Sets the threshold for changing the target in the correction
   equation.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
-  fix - threshold for changing the target

   Options Database Key:
.  -eps_jd_fix - the fix value

   Note:
   The target in the correction equation is fixed at the first iterations.
   When the norm of the residual vector is lower than the fix value,
   the target is set to the corresponding eigenvalue.

   Level: advanced

.seealso: EPSJDGetFix()
@*/
PetscErrorCode EPSJDSetFix(EPS eps,PetscReal fix)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveReal(eps,fix,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetFix_C",(EPS,PetscReal),(eps,fix)));
  PetscFunctionReturn(0);
}

PetscErrorCode EPSJDGetFix_JD(EPS eps,PetscReal *fix)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *fix = data->fix;
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetFix - Returns the threshold for changing the target in the correction
   equation.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameter:
.  fix - threshold for changing the target

   Note:
   The target in the correction equation is fixed at the first iterations.
   When the norm of the residual vector is lower than the fix value,
   the target is set to the corresponding eigenvalue.

   Level: advanced

.seealso: EPSJDSetFix()
@*/
PetscErrorCode EPSJDGetFix(EPS eps,PetscReal *fix)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidRealPointer(fix,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetFix_C",(EPS,PetscReal*),(eps,fix)));
  PetscFunctionReturn(0);
}

PetscErrorCode EPSJDSetConstCorrectionTol_JD(EPS eps,PetscBool constant)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  data->dynamic = PetscNot(constant);
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetConstCorrectionTol - If true, deactivates the dynamic stopping criterion
   (also called Newton) that sets the KSP relative tolerance
   to 0.5**i, where i is the number of EPS iterations from the last converged value.

   Logically Collective on eps

   Input Parameters:
+  eps - the eigenproblem solver context
-  constant - if false, the KSP relative tolerance is set to 0.5**i.

   Options Database Key:
.  -eps_jd_const_correction_tol - Deactivates the dynamic stopping criterion.

   Level: advanced

.seealso: EPSJDGetConstCorrectionTol()
@*/
PetscErrorCode EPSJDSetConstCorrectionTol(EPS eps,PetscBool constant)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveBool(eps,constant,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetConstCorrectionTol_C",(EPS,PetscBool),(eps,constant)));
  PetscFunctionReturn(0);
}

PetscErrorCode EPSJDGetConstCorrectionTol_JD(EPS eps,PetscBool *constant)
{
  EPS_DAVIDSON *data = (EPS_DAVIDSON*)eps->data;

  PetscFunctionBegin;
  *constant = PetscNot(data->dynamic);
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetConstCorrectionTol - Returns a flag indicating if the dynamic stopping is being used for
   solving the correction equation.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameter:
.  constant - boolean flag indicating if the dynamic stopping criterion is not being used.

   Notes:
   If the flag is false the KSP relative tolerance is set to 0.5**i, where i is the number
   of EPS iterations from the last converged value.

   Level: advanced

.seealso: EPSJDSetConstCorrectionTol()
@*/
PetscErrorCode EPSJDGetConstCorrectionTol(EPS eps,PetscBool *constant)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidBoolPointer(constant,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetConstCorrectionTol_C",(EPS,PetscBool*),(eps,constant)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDSetBOrth - Selects the orthogonalization that will be used in the search
   subspace in case of generalized Hermitian problems.

   Logically Collective on eps

   Input Parameters:
+  eps   - the eigenproblem solver context
-  borth - whether to B-orthogonalize the search subspace

   Options Database Key:
.  -eps_jd_borth - Set the orthogonalization used in the search subspace

   Level: advanced

.seealso: EPSJDGetBOrth()
@*/
PetscErrorCode EPSJDSetBOrth(EPS eps,PetscBool borth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidLogicalCollectiveBool(eps,borth,2);
  CHKERRQ(PetscTryMethod(eps,"EPSJDSetBOrth_C",(EPS,PetscBool),(eps,borth)));
  PetscFunctionReturn(0);
}

/*@
   EPSJDGetBOrth - Returns the orthogonalization used in the search
   subspace in case of generalized Hermitian problems.

   Not Collective

   Input Parameter:
.  eps - the eigenproblem solver context

   Output Parameters:
.  borth - whether to B-orthogonalize the search subspace

   Level: advanced

.seealso: EPSJDSetBOrth()
@*/
PetscErrorCode EPSJDGetBOrth(EPS eps,PetscBool *borth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(eps,EPS_CLASSID,1);
  PetscValidBoolPointer(borth,2);
  CHKERRQ(PetscUseMethod(eps,"EPSJDGetBOrth_C",(EPS,PetscBool*),(eps,borth)));
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode EPSCreate_JD(EPS eps)
{
  EPS_DAVIDSON   *data;

  PetscFunctionBegin;
  CHKERRQ(PetscNewLog(eps,&data));
  eps->data = (void*)data;

  data->blocksize   = 1;
  data->initialsize = 0;
  data->minv        = 0;
  data->plusk       = PETSC_DEFAULT;
  data->ipB         = PETSC_TRUE;
  data->fix         = 0.01;
  data->krylovstart = PETSC_FALSE;
  data->dynamic     = PETSC_FALSE;

  eps->useds = PETSC_TRUE;
  eps->categ = EPS_CATEGORY_PRECOND;

  eps->ops->solve          = EPSSolve_XD;
  eps->ops->setup          = EPSSetUp_JD;
  eps->ops->setupsort      = EPSSetUpSort_Default;
  eps->ops->setfromoptions = EPSSetFromOptions_JD;
  eps->ops->destroy        = EPSDestroy_JD;
  eps->ops->reset          = EPSReset_XD;
  eps->ops->view           = EPSView_JD;
  eps->ops->backtransform  = EPSBackTransform_Default;
  eps->ops->computevectors = EPSComputeVectors_XD;
  eps->ops->setdefaultst   = EPSSetDefaultST_JD;

  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetKrylovStart_C",EPSXDSetKrylovStart_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetKrylovStart_C",EPSXDGetKrylovStart_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetBlockSize_C",EPSXDSetBlockSize_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetBlockSize_C",EPSXDGetBlockSize_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetRestart_C",EPSXDSetRestart_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetRestart_C",EPSXDGetRestart_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetInitialSize_C",EPSXDSetInitialSize_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetInitialSize_C",EPSXDGetInitialSize_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetFix_C",EPSJDSetFix_JD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetFix_C",EPSJDGetFix_JD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetConstCorrectionTol_C",EPSJDSetConstCorrectionTol_JD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetConstCorrectionTol_C",EPSJDGetConstCorrectionTol_JD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDSetBOrth_C",EPSXDSetBOrth_XD));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)eps,"EPSJDGetBOrth_C",EPSXDGetBOrth_XD));
  PetscFunctionReturn(0);
}
