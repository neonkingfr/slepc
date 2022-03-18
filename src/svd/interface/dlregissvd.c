/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

#include <slepc/private/svdimpl.h>

static PetscBool SVDPackageInitialized = PETSC_FALSE;

const char *SVDTRLanczosGBidiags[] = {"SINGLE","UPPER","LOWER","SVDTRLanczosGBidiag","SVD_TRLANCZOS_GBIDIAG_",0};
const char *SVDErrorTypes[] = {"ABSOLUTE","RELATIVE","SVDErrorType","SVD_ERROR_",0};
const char *SVDPRIMMEMethods[] = {"","HYBRID","NORMALEQUATIONS","AUGMENTED","SVDPRIMMEMethod","SVD_PRIMME_",0};
const char *const SVDConvergedReasons_Shifted[] = {"","","DIVERGED_BREAKDOWN","DIVERGED_ITS","CONVERGED_ITERATING","CONVERGED_TOL","CONVERGED_USER","CONVERGED_MAXIT","SVDConvergedReason","SVD_",0};
const char *const*SVDConvergedReasons = SVDConvergedReasons_Shifted + 4;

/*@C
   SVDFinalizePackage - This function destroys everything in the Slepc interface
   to the SVD package. It is called from SlepcFinalize().

   Level: developer

.seealso: SlepcFinalize()
@*/
PetscErrorCode SVDFinalizePackage(void)
{
  PetscFunctionBegin;
  CHKERRQ(PetscFunctionListDestroy(&SVDList));
  CHKERRQ(PetscFunctionListDestroy(&SVDMonitorList));
  CHKERRQ(PetscFunctionListDestroy(&SVDMonitorCreateList));
  CHKERRQ(PetscFunctionListDestroy(&SVDMonitorDestroyList));
  SVDPackageInitialized       = PETSC_FALSE;
  SVDRegisterAllCalled        = PETSC_FALSE;
  SVDMonitorRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

/*@C
   SVDInitializePackage - This function initializes everything in the SVD package.
   It is called from PetscDLLibraryRegister() when using dynamic libraries, and
   on the first call to SVDCreate() when using static libraries.

   Level: developer

.seealso: SlepcInitialize()
@*/
PetscErrorCode SVDInitializePackage(void)
{
  char           logList[256];
  PetscBool      opt,pkg;
  PetscClassId   classids[1];

  PetscFunctionBegin;
  if (SVDPackageInitialized) PetscFunctionReturn(0);
  SVDPackageInitialized = PETSC_TRUE;
  /* Register Classes */
  CHKERRQ(PetscClassIdRegister("SVD Solver",&SVD_CLASSID));
  /* Register Constructors */
  CHKERRQ(SVDRegisterAll());
  /* Register Monitors */
  CHKERRQ(SVDMonitorRegisterAll());
  /* Register Events */
  CHKERRQ(PetscLogEventRegister("SVDSetUp",SVD_CLASSID,&SVD_SetUp));
  CHKERRQ(PetscLogEventRegister("SVDSolve",SVD_CLASSID,&SVD_Solve));
  /* Process Info */
  classids[0] = SVD_CLASSID;
  CHKERRQ(PetscInfoProcessClass("svd",1,&classids[0]));
  /* Process summary exclusions */
  CHKERRQ(PetscOptionsGetString(NULL,NULL,"-log_exclude",logList,sizeof(logList),&opt));
  if (opt) {
    CHKERRQ(PetscStrInList("svd",logList,',',&pkg));
    if (pkg) CHKERRQ(PetscLogEventDeactivateClass(SVD_CLASSID));
  }
  /* Register package finalizer */
  CHKERRQ(PetscRegisterFinalize(SVDFinalizePackage));
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_DYNAMIC_LIBRARIES)
/*
  PetscDLLibraryRegister - This function is called when the dynamic library
  it is in is opened.

  This one registers all the SVD methods that are in the basic SLEPc libslepcsvd
  library.
 */
SLEPC_EXTERN PetscErrorCode PetscDLLibraryRegister_slepcsvd()
{
  PetscFunctionBegin;
  CHKERRQ(SVDInitializePackage());
  PetscFunctionReturn(0);
}
#endif /* PETSC_HAVE_DYNAMIC_LIBRARIES */
