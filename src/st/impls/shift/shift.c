/*
    Shift spectral transformation, applies (A + sigma I) as operator, or
    inv(B)(A + sigma B) for generalized problems

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

#include <slepc-private/stimpl.h>          /*I "slepcst.h" I*/

#undef __FUNCT__
#define __FUNCT__ "STApply_Shift"
PetscErrorCode STApply_Shift(ST st,Vec x,Vec y)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (st->nmat>1) {
    /* generalized eigenproblem: y = B^-1 (A - sB) x */
    ierr = MatMult(st->T[0],x,st->w);CHKERRQ(ierr);
    ierr = STMatSolve(st,st->w,y);CHKERRQ(ierr);
  } else {
    /* standard eigenproblem: y = (A - sI) x */
    ierr = MatMult(st->T[0],x,y);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STApplyTranspose_Shift"
PetscErrorCode STApplyTranspose_Shift(ST st,Vec x,Vec y)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (st->nmat>1) {
    /* generalized eigenproblem: y = (A - sB)^T B^-T  x */
    ierr = STMatSolveTranspose(st,x,st->w);CHKERRQ(ierr);
    ierr = MatMultTranspose(st->T[0],st->w,y);CHKERRQ(ierr);
  } else {
    /* standard eigenproblem: y = (A^T - sI) x */
    ierr = MatMultTranspose(st->T[0],x,y);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STBackTransform_Shift"
PetscErrorCode STBackTransform_Shift(ST st,PetscInt n,PetscScalar *eigr,PetscScalar *eigi)
{
  PetscInt j;

  PetscFunctionBegin;
  for (j=0;j<n;j++) {
    eigr[j] += st->sigma;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STPostSolve_Shift"
PetscErrorCode STPostSolve_Shift(ST st)
{
  PetscErrorCode ierr;
  PetscScalar    s;

  PetscFunctionBegin;
  if (st->shift_matrix == ST_MATMODE_INPLACE) {
    if (st->nmat>1) {
      if (st->nmat==3) {
        ierr = MatAXPY(st->A[0],-st->sigma*st->sigma,st->A[2],st->str);CHKERRQ(ierr);
        ierr = MatAXPY(st->A[1],2.0*st->sigma,st->A[2],st->str);CHKERRQ(ierr);
        s = st->sigma;
      } else s = st->sigma;
      ierr = MatAXPY(st->A[0],s,st->A[1],st->str);CHKERRQ(ierr);
    } else {
      ierr = MatShift(st->A[0],st->sigma);CHKERRQ(ierr);
    }
    st->Astate[0] = ((PetscObject)st->A[0])->state;
    st->setupcalled = 0;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STSetUp_Shift"
PetscErrorCode STSetUp_Shift(ST st)
{
  PetscErrorCode ierr;
  PetscInt       k,nc,nmat=st->nmat;
  PetscScalar    *coeffs;

  PetscFunctionBegin;
  if (nmat<3) {
    /* T[1] = B */
    if (nmat>1) { ierr = PetscObjectReference((PetscObject)st->A[1]);CHKERRQ(ierr); }
    st->T[1] = st->A[1];
    /* T[0] = A-sigma*B  */
    ierr = STMatGAXPY_Private(st,-st->sigma,0.0,1,0,PETSC_TRUE);CHKERRQ(ierr);
  } else {
    if (st->transform) {
      nc = (nmat*(nmat+1))/2;
      ierr = PetscMalloc(nc*sizeof(PetscScalar),&coeffs);CHKERRQ(ierr);
      /* Compute coeffs */
      ierr = STCoeffs_Monomial(st,coeffs);CHKERRQ(ierr);
      /* T[n] = A_n */
      k = nmat-1;
      ierr = PetscObjectReference((PetscObject)st->A[k]);CHKERRQ(ierr);
      st->T[k] = st->A[k];
      for (k=0;k<nmat-1;k++) {
        ierr = STMatMAXPY_Private(st,st->sigma,k,coeffs+((nmat-k)*(nmat-k-1))/2,PETSC_TRUE,&st->T[k],PETSC_FALSE);CHKERRQ(ierr);
      }
      ierr = PetscFree(coeffs);CHKERRQ(ierr);
    } else {
      for (k=0;k<nmat;k++) {
        ierr = PetscObjectReference((PetscObject)st->A[k]);CHKERRQ(ierr);
        st->T[k] = st->A[k];
      }
    }
  }
  if (nmat>1) {
    st->P = st->T[nmat-1];
    ierr = PetscObjectReference((PetscObject)st->P);CHKERRQ(ierr);
  }
  if (st->P) {
    if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
    ierr = KSPSetOperators(st->ksp,st->P,st->P,DIFFERENT_NONZERO_PATTERN);CHKERRQ(ierr);
    ierr = KSPSetUp(st->ksp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STSetShift_Shift"
PetscErrorCode STSetShift_Shift(ST st,PetscScalar newshift)
{
  PetscErrorCode ierr;
  PetscInt       k,nc,nmat=st->nmat;
  PetscScalar    *coeffs;

  PetscFunctionBegin;
  /* Nothing to be done if STSetUp has not been called yet */
  if (!st->setupcalled) PetscFunctionReturn(0);

  if (st->nmat<3) {
    ierr = STMatGAXPY_Private(st,newshift,-st->sigma,1,0,PETSC_FALSE);CHKERRQ(ierr);
  } else {
    if (st->transform) {
      if (st->shift_matrix == ST_MATMODE_COPY) {
        nc = (nmat*(nmat+1))/2;
        ierr = PetscMalloc(nc*sizeof(PetscScalar),&coeffs);CHKERRQ(ierr);
        /* Compute coeffs */
        ierr = STCoeffs_Monomial(st,coeffs);CHKERRQ(ierr);
        for (k=0;k<nmat-1;k++) {
          ierr = STMatMAXPY_Private(st,newshift,k,coeffs+((nmat-k)*(nmat-k-1))/2,PETSC_FALSE,&st->T[k],PETSC_FALSE);CHKERRQ(ierr);
        }
        ierr = PetscFree(coeffs);CHKERRQ(ierr);
      } else {
        for (k=0;k<nmat-1;k++) {
          ierr = STMatMAXPY_Private(st,newshift,k,NULL,PETSC_FALSE,&st->T[k],PETSC_FALSE);CHKERRQ(ierr);
        }
      } 
    } 
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STSetFromOptions_Shift"
PetscErrorCode STSetFromOptions_Shift(ST st)
{
  PetscErrorCode ierr;
  PC             pc;
  PCType         pctype;
  KSPType        ksptype;

  PetscFunctionBegin;
  if (!st->ksp) { ierr = STGetKSP(st,&st->ksp);CHKERRQ(ierr); }
  ierr = KSPGetPC(st->ksp,&pc);CHKERRQ(ierr);
  ierr = KSPGetType(st->ksp,&ksptype);CHKERRQ(ierr);
  ierr = PCGetType(pc,&pctype);CHKERRQ(ierr);
  if (!pctype && !ksptype) {
    if (st->shift_matrix == ST_MATMODE_SHELL) {
      /* in shell mode use GMRES with Jacobi as the default */
      ierr = KSPSetType(st->ksp,KSPGMRES);CHKERRQ(ierr);
      ierr = PCSetType(pc,PCJACOBI);CHKERRQ(ierr);
    } else {
      /* use direct solver as default */
      ierr = KSPSetType(st->ksp,KSPPREONLY);CHKERRQ(ierr);
      ierr = PCSetType(pc,PCREDUNDANT);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "STCreate_Shift"
PETSC_EXTERN PetscErrorCode STCreate_Shift(ST st)
{
  PetscFunctionBegin;
  st->ops->apply           = STApply_Shift;
  st->ops->getbilinearform = STGetBilinearForm_Default;
  st->ops->applytrans      = STApplyTranspose_Shift;
  st->ops->postsolve       = STPostSolve_Shift;
  st->ops->backtransform   = STBackTransform_Shift;
  st->ops->setfromoptions  = STSetFromOptions_Shift;
  st->ops->setup           = STSetUp_Shift;
  st->ops->setshift        = STSetShift_Shift;
  PetscFunctionReturn(0);
}
