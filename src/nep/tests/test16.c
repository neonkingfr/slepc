/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Illustrates use of NEPSetEigenvalueComparison().\n\n"
  "This is a simplified version of ex20.\n"
  "The command line options are:\n"
  "  -n <n>, where <n> = number of grid subdivisions.\n";

/*
   Solve 1-D PDE
            -u'' = lambda*u
   on [0,1] subject to
            u(0)=0, u'(1)=u(1)*lambda*kappa/(kappa-lambda)
*/

#include <slepcnep.h>

/*
   User-defined routines
*/
PetscErrorCode FormFunction(NEP,PetscScalar,Mat,Mat,void*);
PetscErrorCode FormJacobian(NEP,PetscScalar,Mat,void*);
PetscErrorCode MyEigenSort(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*);

/*
   User-defined application context
*/
typedef struct {
  PetscScalar kappa;   /* ratio between stiffness of spring and attached mass */
  PetscReal   h;       /* mesh spacing */
} ApplicationCtx;

int main(int argc,char **argv)
{
  NEP            nep;             /* nonlinear eigensolver context */
  Mat            F,J;             /* Function and Jacobian matrices */
  ApplicationCtx ctx;             /* user-defined context */
  PetscScalar    target;
  RG             rg;
  PetscInt       n=128;
  PetscBool      terse;
  PetscErrorCode ierr;

  ierr = SlepcInitialize(&argc,&argv,(char*)0,help);if (ierr) return ierr;
  CHKERRQ(PetscOptionsGetInt(NULL,NULL,"-n",&n,NULL));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"\n1-D Nonlinear Eigenproblem, n=%" PetscInt_FMT "\n\n",n));
  ctx.h = 1.0/(PetscReal)n;
  ctx.kappa = 1.0;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
               Prepare nonlinear eigensolver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(NEPCreate(PETSC_COMM_WORLD,&nep));

  CHKERRQ(MatCreate(PETSC_COMM_WORLD,&F));
  CHKERRQ(MatSetSizes(F,PETSC_DECIDE,PETSC_DECIDE,n,n));
  CHKERRQ(MatSetFromOptions(F));
  CHKERRQ(MatSeqAIJSetPreallocation(F,3,NULL));
  CHKERRQ(MatMPIAIJSetPreallocation(F,3,NULL,1,NULL));
  CHKERRQ(MatSetUp(F));
  CHKERRQ(NEPSetFunction(nep,F,F,FormFunction,&ctx));

  CHKERRQ(MatCreate(PETSC_COMM_WORLD,&J));
  CHKERRQ(MatSetSizes(J,PETSC_DECIDE,PETSC_DECIDE,n,n));
  CHKERRQ(MatSetFromOptions(J));
  CHKERRQ(MatSeqAIJSetPreallocation(J,3,NULL));
  CHKERRQ(MatMPIAIJSetPreallocation(F,3,NULL,1,NULL));
  CHKERRQ(MatSetUp(J));
  CHKERRQ(NEPSetJacobian(nep,J,FormJacobian,&ctx));

  CHKERRQ(NEPSetType(nep,NEPNLEIGS));
  CHKERRQ(NEPGetRG(nep,&rg));
  CHKERRQ(RGSetType(rg,RGINTERVAL));
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(RGIntervalSetEndpoints(rg,2.0,400.0,-0.001,0.001));
#else
  CHKERRQ(RGIntervalSetEndpoints(rg,2.0,400.0,0,0));
#endif
  CHKERRQ(NEPSetTarget(nep,25.0));
  CHKERRQ(NEPSetEigenvalueComparison(nep,MyEigenSort,&target));
  CHKERRQ(NEPSetTolerances(nep,PETSC_SMALL,PETSC_DEFAULT));
  CHKERRQ(NEPSetFromOptions(nep));
  CHKERRQ(NEPGetTarget(nep,&target));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
              Solve the eigensystem and display the solution
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(NEPSolve(nep));

  /* show detailed info unless -terse option is given by user */
  CHKERRQ(PetscOptionsHasName(NULL,NULL,"-terse",&terse));
  if (terse) {
    CHKERRQ(NEPErrorView(nep,NEP_ERROR_RELATIVE,NULL));
  } else {
    CHKERRQ(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_INFO_DETAIL));
    CHKERRQ(NEPConvergedReasonView(nep,PETSC_VIEWER_STDOUT_WORLD));
    CHKERRQ(NEPErrorView(nep,NEP_ERROR_RELATIVE,PETSC_VIEWER_STDOUT_WORLD));
    CHKERRQ(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
  }

  CHKERRQ(NEPDestroy(&nep));
  CHKERRQ(MatDestroy(&F));
  CHKERRQ(MatDestroy(&J));
  ierr = SlepcFinalize();
  return ierr;
}

/* ------------------------------------------------------------------- */
/*
   FormFunction - Computes Function matrix  T(lambda)

   Input Parameters:
.  nep    - the NEP context
.  lambda - the scalar argument
.  ctx    - optional user-defined context, as set by NEPSetFunction()

   Output Parameters:
.  fun - Function matrix
.  B   - optionally different preconditioning matrix
*/
PetscErrorCode FormFunction(NEP nep,PetscScalar lambda,Mat fun,Mat B,void *ctx)
{
  ApplicationCtx *user = (ApplicationCtx*)ctx;
  PetscScalar    A[3],c,d;
  PetscReal      h;
  PetscInt       i,n,j[3],Istart,Iend;
  PetscBool      FirstBlock=PETSC_FALSE,LastBlock=PETSC_FALSE;

  PetscFunctionBeginUser;
  /*
     Compute Function entries and insert into matrix
  */
  CHKERRQ(MatGetSize(fun,&n,NULL));
  CHKERRQ(MatGetOwnershipRange(fun,&Istart,&Iend));
  if (Istart==0) FirstBlock=PETSC_TRUE;
  if (Iend==n) LastBlock=PETSC_TRUE;
  h = user->h;
  c = user->kappa/(lambda-user->kappa);
  d = n;

  /*
     Interior grid points
  */
  for (i=(FirstBlock? Istart+1: Istart);i<(LastBlock? Iend-1: Iend);i++) {
    j[0] = i-1; j[1] = i; j[2] = i+1;
    A[0] = A[2] = -d-lambda*h/6.0; A[1] = 2.0*(d-lambda*h/3.0);
    CHKERRQ(MatSetValues(fun,1,&i,3,j,A,INSERT_VALUES));
  }

  /*
     Boundary points
  */
  if (FirstBlock) {
    i = 0;
    j[0] = 0; j[1] = 1;
    A[0] = 2.0*(d-lambda*h/3.0); A[1] = -d-lambda*h/6.0;
    CHKERRQ(MatSetValues(fun,1,&i,2,j,A,INSERT_VALUES));
  }

  if (LastBlock) {
    i = n-1;
    j[0] = n-2; j[1] = n-1;
    A[0] = -d-lambda*h/6.0; A[1] = d-lambda*h/3.0+lambda*c;
    CHKERRQ(MatSetValues(fun,1,&i,2,j,A,INSERT_VALUES));
  }

  /*
     Assemble matrix
  */
  CHKERRQ(MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY));
  CHKERRQ(MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY));
  if (fun != B) {
    CHKERRQ(MatAssemblyBegin(fun,MAT_FINAL_ASSEMBLY));
    CHKERRQ(MatAssemblyEnd(fun,MAT_FINAL_ASSEMBLY));
  }
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------- */
/*
   FormJacobian - Computes Jacobian matrix  T'(lambda)

   Input Parameters:
.  nep    - the NEP context
.  lambda - the scalar argument
.  ctx    - optional user-defined context, as set by NEPSetJacobian()

   Output Parameters:
.  jac - Jacobian matrix
.  B   - optionally different preconditioning matrix
*/
PetscErrorCode FormJacobian(NEP nep,PetscScalar lambda,Mat jac,void *ctx)
{
  ApplicationCtx *user = (ApplicationCtx*)ctx;
  PetscScalar    A[3],c;
  PetscReal      h;
  PetscInt       i,n,j[3],Istart,Iend;
  PetscBool      FirstBlock=PETSC_FALSE,LastBlock=PETSC_FALSE;

  PetscFunctionBeginUser;
  /*
     Compute Jacobian entries and insert into matrix
  */
  CHKERRQ(MatGetSize(jac,&n,NULL));
  CHKERRQ(MatGetOwnershipRange(jac,&Istart,&Iend));
  if (Istart==0) FirstBlock=PETSC_TRUE;
  if (Iend==n) LastBlock=PETSC_TRUE;
  h = user->h;
  c = user->kappa/(lambda-user->kappa);

  /*
     Interior grid points
  */
  for (i=(FirstBlock? Istart+1: Istart);i<(LastBlock? Iend-1: Iend);i++) {
    j[0] = i-1; j[1] = i; j[2] = i+1;
    A[0] = A[2] = -h/6.0; A[1] = -2.0*h/3.0;
    CHKERRQ(MatSetValues(jac,1,&i,3,j,A,INSERT_VALUES));
  }

  /*
     Boundary points
  */
  if (FirstBlock) {
    i = 0;
    j[0] = 0; j[1] = 1;
    A[0] = -2.0*h/3.0; A[1] = -h/6.0;
    CHKERRQ(MatSetValues(jac,1,&i,2,j,A,INSERT_VALUES));
  }

  if (LastBlock) {
    i = n-1;
    j[0] = n-2; j[1] = n-1;
    A[0] = -h/6.0; A[1] = -h/3.0-c*c;
    CHKERRQ(MatSetValues(jac,1,&i,2,j,A,INSERT_VALUES));
  }

  /*
     Assemble matrix
  */
  CHKERRQ(MatAssemblyBegin(jac,MAT_FINAL_ASSEMBLY));
  CHKERRQ(MatAssemblyEnd(jac,MAT_FINAL_ASSEMBLY));
  PetscFunctionReturn(0);
}

/*
    Function for user-defined eigenvalue ordering criterion.

    Given two eigenvalues ar+i*ai and br+i*bi, the subroutine must choose
    one of them as the preferred one according to the criterion.
    In this example, eigenvalues are sorted with respect to the target,
    but those on the right of the target are preferred.
*/
PetscErrorCode MyEigenSort(PetscScalar ar,PetscScalar ai,PetscScalar br,PetscScalar bi,PetscInt *r,void *ctx)
{
  PetscReal   a,b;
  PetscScalar target = *(PetscScalar*)ctx;

  PetscFunctionBeginUser;
  if (PetscRealPart(ar-target)<0.0 && PetscRealPart(br-target)>0.0) *r = 1;
  else {
    a = SlepcAbsEigenvalue(ar-target,ai);
    b = SlepcAbsEigenvalue(br-target,bi);
    if (a>b) *r = 1;
    else if (a<b) *r = -1;
    else *r = 0;
  }
  PetscFunctionReturn(0);
}

/*TEST

   test:
      suffix: 1
      args: -nep_nev 4 -nep_ncv 8 -terse
      requires: double !complex

TEST*/
