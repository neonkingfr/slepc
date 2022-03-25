/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Illustrates the use of shell spectral transformations. "
  "The problem to be solved is the same as ex1.c and"
  "corresponds to the Laplacian operator in 1 dimension.\n\n"
  "The command line options are:\n"
  "  -n <n>, where <n> = number of grid subdivisions = matrix dimension.\n\n";

#include <slepceps.h>

/* Define context for user-provided spectral transformation */
typedef struct {
  KSP        ksp;
} SampleShellST;

/* Declare routines for user-provided spectral transformation */
PetscErrorCode STCreate_User(SampleShellST**);
PetscErrorCode STSetUp_User(SampleShellST*,ST);
PetscErrorCode STApply_User(ST,Vec,Vec);
PetscErrorCode STApplyTranspose_User(ST,Vec,Vec);
PetscErrorCode STBackTransform_User(ST,PetscInt,PetscScalar*,PetscScalar*);
PetscErrorCode STDestroy_User(SampleShellST*);

int main (int argc,char **argv)
{
  Mat            A;               /* operator matrix */
  EPS            eps;             /* eigenproblem solver context */
  ST             st;              /* spectral transformation context */
  SampleShellST  *shell;          /* user-defined spectral transform context */
  EPSType        type;
  PetscInt       n=30,i,Istart,Iend,nev;
  PetscBool      isShell,terse;

  CHKERRQ(SlepcInitialize(&argc,&argv,(char*)0,help));

  CHKERRQ(PetscOptionsGetInt(NULL,NULL,"-n",&n,NULL));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"\n1-D Laplacian Eigenproblem (shell-enabled), n=%" PetscInt_FMT "\n\n",n));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Compute the operator matrix that defines the eigensystem, Ax=kx
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(MatCreate(PETSC_COMM_WORLD,&A));
  CHKERRQ(MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,n,n));
  CHKERRQ(MatSetFromOptions(A));
  CHKERRQ(MatSetUp(A));

  CHKERRQ(MatGetOwnershipRange(A,&Istart,&Iend));
  for (i=Istart;i<Iend;i++) {
    if (i>0) CHKERRQ(MatSetValue(A,i,i-1,-1.0,INSERT_VALUES));
    if (i<n-1) CHKERRQ(MatSetValue(A,i,i+1,-1.0,INSERT_VALUES));
    CHKERRQ(MatSetValue(A,i,i,2.0,INSERT_VALUES));
  }
  CHKERRQ(MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY));
  CHKERRQ(MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                Create the eigensolver and set various options
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*
     Create eigensolver context
  */
  CHKERRQ(EPSCreate(PETSC_COMM_WORLD,&eps));

  /*
     Set operators. In this case, it is a standard eigenvalue problem
  */
  CHKERRQ(EPSSetOperators(eps,A,NULL));
  CHKERRQ(EPSSetProblemType(eps,EPS_HEP));

  /*
     Set solver parameters at runtime
  */
  CHKERRQ(EPSSetFromOptions(eps));

  /*
     Initialize shell spectral transformation if selected by user
  */
  CHKERRQ(EPSGetST(eps,&st));
  CHKERRQ(PetscObjectTypeCompare((PetscObject)st,STSHELL,&isShell));
  if (isShell) {
    /* Change sorting criterion since this ST example computes values
       closest to 0 */
    CHKERRQ(EPSSetWhichEigenpairs(eps,EPS_SMALLEST_REAL));

    /* (Required) Create a context for the user-defined spectral transform;
       this context can be defined to contain any application-specific data. */
    CHKERRQ(STCreate_User(&shell));
    CHKERRQ(STShellSetContext(st,shell));

    /* (Required) Set the user-defined routine for applying the operator */
    CHKERRQ(STShellSetApply(st,STApply_User));

    /* (Optional) Set the user-defined routine for applying the transposed operator */
    CHKERRQ(STShellSetApplyTranspose(st,STApplyTranspose_User));

    /* (Optional) Set the user-defined routine for back-transformation */
    CHKERRQ(STShellSetBackTransform(st,STBackTransform_User));

    /* (Optional) Set a name for the transformation, used for STView() */
    CHKERRQ(PetscObjectSetName((PetscObject)st,"MyTransformation"));

    /* (Optional) Do any setup required for the new transformation */
    CHKERRQ(STSetUp_User(shell,st));
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                      Solve the eigensystem
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(EPSSolve(eps));

  /*
     Optional: Get some information from the solver and display it
  */
  CHKERRQ(EPSGetType(eps,&type));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD," Solution method: %s\n\n",type));
  CHKERRQ(EPSGetDimensions(eps,&nev,NULL,NULL));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD," Number of requested eigenvalues: %" PetscInt_FMT "\n",nev));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                    Display solution and clean up
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* show detailed info unless -terse option is given by user */
  CHKERRQ(PetscOptionsHasName(NULL,NULL,"-terse",&terse));
  if (terse) CHKERRQ(EPSErrorView(eps,EPS_ERROR_RELATIVE,NULL));
  else {
    CHKERRQ(PetscViewerPushFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_INFO_DETAIL));
    CHKERRQ(EPSConvergedReasonView(eps,PETSC_VIEWER_STDOUT_WORLD));
    CHKERRQ(EPSErrorView(eps,EPS_ERROR_RELATIVE,PETSC_VIEWER_STDOUT_WORLD));
    CHKERRQ(PetscViewerPopFormat(PETSC_VIEWER_STDOUT_WORLD));
  }
  if (isShell) CHKERRQ(STDestroy_User(shell));
  CHKERRQ(EPSDestroy(&eps));
  CHKERRQ(MatDestroy(&A));
  CHKERRQ(SlepcFinalize());
  return 0;
}

/***********************************************************************/
/*     Routines for a user-defined shell spectral transformation       */
/***********************************************************************/

/*
   STCreate_User - This routine creates a user-defined
   spectral transformation context.

   Output Parameter:
.  shell - user-defined spectral transformation context
*/
PetscErrorCode STCreate_User(SampleShellST **shell)
{
  SampleShellST  *newctx;

  PetscFunctionBeginUser;
  CHKERRQ(PetscNew(&newctx));
  CHKERRQ(KSPCreate(PETSC_COMM_WORLD,&newctx->ksp));
  CHKERRQ(KSPAppendOptionsPrefix(newctx->ksp,"st_"));
  *shell = newctx;
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
/*
   STSetUp_User - This routine sets up a user-defined
   spectral transformation context.

   Input Parameters:
+  shell - user-defined spectral transformation context
-  st    - spectral transformation context containing the operator matrices

   Output Parameter:
.  shell - fully set up user-defined transformation context

   Notes:
   In this example, the user-defined transformation is simply OP=A^-1.
   Therefore, the eigenpairs converge in reversed order. The KSP object
   used for the solution of linear systems with A is handled via the
   user-defined context SampleShellST.
*/
PetscErrorCode STSetUp_User(SampleShellST *shell,ST st)
{
  Mat            A;

  PetscFunctionBeginUser;
  CHKERRQ(STGetMatrix(st,0,&A));
  CHKERRQ(KSPSetOperators(shell->ksp,A,A));
  CHKERRQ(KSPSetFromOptions(shell->ksp));
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
/*
   STApply_User - This routine demonstrates the use of a
   user-provided spectral transformation.

   Input Parameters:
+  st - spectral transformation context
-  x - input vector

   Output Parameter:
.  y - output vector

   Notes:
   The transformation implemented in this code is just OP=A^-1 and
   therefore it is of little use, merely as an example of working with
   a STSHELL.
*/
PetscErrorCode STApply_User(ST st,Vec x,Vec y)
{
  SampleShellST  *shell;

  PetscFunctionBeginUser;
  CHKERRQ(STShellGetContext(st,&shell));
  CHKERRQ(KSPSolve(shell->ksp,x,y));
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
/*
   STApplyTranspose_User - This is not required unless using a two-sided
   eigensolver.

   Input Parameters:
+  st - spectral transformation context
-  x - input vector

   Output Parameter:
.  y - output vector
*/
PetscErrorCode STApplyTranspose_User(ST st,Vec x,Vec y)
{
  SampleShellST  *shell;

  PetscFunctionBeginUser;
  CHKERRQ(STShellGetContext(st,&shell));
  CHKERRQ(KSPSolveTranspose(shell->ksp,x,y));
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
/*
   STBackTransform_User - This routine demonstrates the use of a
   user-provided spectral transformation.

   Input Parameters:
+  st - spectral transformation context
-  n  - number of eigenvalues to transform

   Input/Output Parameters:
+  eigr - pointer to real part of eigenvalues
-  eigi - pointer to imaginary part of eigenvalues

   Notes:
   This code implements the back transformation of eigenvalues in
   order to retrieve the eigenvalues of the original problem. In this
   example, simply set k_i = 1/k_i.
*/
PetscErrorCode STBackTransform_User(ST st,PetscInt n,PetscScalar *eigr,PetscScalar *eigi)
{
  PetscInt j;

  PetscFunctionBeginUser;
  for (j=0;j<n;j++) {
    eigr[j] = 1.0 / eigr[j];
  }
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
/*
   STDestroy_User - This routine destroys a user-defined
   spectral transformation context.

   Input Parameter:
.  shell - user-defined spectral transformation context
*/
PetscErrorCode STDestroy_User(SampleShellST *shell)
{
  PetscFunctionBeginUser;
  CHKERRQ(KSPDestroy(&shell->ksp));
  CHKERRQ(PetscFree(shell));
  PetscFunctionReturn(0);
}

/*TEST

   testset:
      args: -eps_nev 5 -eps_non_hermitian -terse
      output_file: output/ex10_1.out
      test:
         suffix: 1_sinvert
         args: -st_type sinvert
      test:
         suffix: 1_sinvert_twoside
         args: -st_type sinvert -eps_balance twoside
         requires: !single
      test:
         suffix: 1_shell
         args: -st_type shell
         requires: !single
      test:
         suffix: 1_shell_twoside
         args: -st_type shell -eps_balance twoside

TEST*/
