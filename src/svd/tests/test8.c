/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/

static char help[] = "Tests multiple calls to SVDSolve changing ncv.\n\n"
  "The command line options are:\n"
  "  -n <n>, where <n> = matrix dimension.\n\n";

#include <slepcsvd.h>

/*
   This example computes the singular values of an nxn Grcar matrix,
   which is a nonsymmetric Toeplitz matrix:

              |  1  1  1  1               |
              | -1  1  1  1  1            |
              |    -1  1  1  1  1         |
              |       .  .  .  .  .       |
          A = |          .  .  .  .  .    |
              |            -1  1  1  1  1 |
              |               -1  1  1  1 |
              |                  -1  1  1 |
              |                     -1  1 |

 */

int main(int argc,char **argv)
{
  Mat            A;
  SVD            svd;
  PetscInt       N=30,Istart,Iend,i,col[5],nsv,ncv;
  PetscScalar    value[] = { -1, 1, 1, 1, 1 };
  PetscErrorCode ierr;

  ierr = SlepcInitialize(&argc,&argv,(char*)0,help);if (ierr) return ierr;
  CHKERRQ(PetscOptionsGetInt(NULL,NULL,"-n",&N,NULL));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"\nSingular values of a Grcar matrix, n=%" PetscInt_FMT,N));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD,"\n\n"));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        Generate the matrix
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(MatCreate(PETSC_COMM_WORLD,&A));
  CHKERRQ(MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,N,N));
  CHKERRQ(MatSetFromOptions(A));
  CHKERRQ(MatSetUp(A));

  CHKERRQ(MatGetOwnershipRange(A,&Istart,&Iend));
  for (i=Istart;i<Iend;i++) {
    col[0]=i-1; col[1]=i; col[2]=i+1; col[3]=i+2; col[4]=i+3;
    if (i==0) {
      CHKERRQ(MatSetValues(A,1,&i,4,col+1,value+1,INSERT_VALUES));
    } else {
      CHKERRQ(MatSetValues(A,1,&i,PetscMin(5,N-i+1),col,value,INSERT_VALUES));
    }
  }

  CHKERRQ(MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY));
  CHKERRQ(MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
         Create the singular value solver and set the solution method
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  CHKERRQ(SVDCreate(PETSC_COMM_WORLD,&svd));
  CHKERRQ(SVDSetOperators(svd,A,NULL));
  CHKERRQ(SVDSetTolerances(svd,1e-6,1000));
  CHKERRQ(SVDSetWhichSingularTriplets(svd,SVD_LARGEST));
  CHKERRQ(SVDSetFromOptions(svd));

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                      Compute the singular values
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* First solve */
  CHKERRQ(SVDSolve(svd));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD," - - - First solve, default subspace dimension - - -\n"));
  CHKERRQ(SVDErrorView(svd,SVD_ERROR_RELATIVE,NULL));

  /* Second solve */
  CHKERRQ(SVDGetDimensions(svd,&nsv,&ncv,NULL));
  CHKERRQ(SVDSetDimensions(svd,nsv,ncv+2,PETSC_DEFAULT));
  CHKERRQ(SVDSolve(svd));
  CHKERRQ(PetscPrintf(PETSC_COMM_WORLD," - - - Second solve, subspace of increased size - - -\n"));
  CHKERRQ(SVDErrorView(svd,SVD_ERROR_RELATIVE,NULL));

  /* Free work space */
  CHKERRQ(SVDDestroy(&svd));
  CHKERRQ(MatDestroy(&A));
  ierr = SlepcFinalize();
  return ierr;
}

/*TEST

   test:
      suffix: 1
      args: -svd_type {{lanczos trlanczos cross cyclic lapack randomized}} -svd_nsv 3 -svd_ncv 12
      requires: !single

TEST*/
