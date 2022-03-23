/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   SLEPc eigensolver: "davidson"

   Step: initialize subspace V
*/

#include "davidson.h"

typedef struct {
  PetscInt k;                 /* desired initial subspace size */
  PetscInt user;              /* number of user initial vectors */
  void     *old_initV_data;   /* old initV data */
} dvdInitV;

static PetscErrorCode dvd_initV_classic_0(dvdDashboard *d)
{
  dvdInitV       *data = (dvdInitV*)d->initV_data;
  PetscInt       i,user = PetscMin(data->user,d->eps->mpd), l,k;

  PetscFunctionBegin;
  CHKERRQ(BVGetActiveColumns(d->eps->V,&l,&k));
  /* User vectors are added at the beginning, so no active column should be in V */
  PetscAssert(data->user==0 || l==0,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Consistency broken");
  /* Generate a set of random initial vectors and orthonormalize them */
  for (i=l+user;i<l+data->k && i<d->eps->ncv && i-l<d->eps->mpd;i++) CHKERRQ(BVSetRandomColumn(d->eps->V,i));
  d->V_tra_s = 0; d->V_tra_e = 0;
  d->V_new_s = 0; d->V_new_e = i-l;

  /* After that the user vectors will be destroyed */
  data->user = 0;
  PetscFunctionReturn(0);
}

static PetscErrorCode dvd_initV_krylov_0(dvdDashboard *d)
{
  dvdInitV       *data = (dvdInitV*)d->initV_data;
  PetscInt       i,user = PetscMin(data->user,d->eps->mpd),l,k;
  Vec            av,v1,v2;

  PetscFunctionBegin;
  CHKERRQ(BVGetActiveColumns(d->eps->V,&l,&k));
  /* User vectors are added at the beginning, so no active column should be in V */
  PetscAssert(data->user==0 || l==0,PETSC_COMM_SELF,PETSC_ERR_PLIB,"Consistency broken");

  /* If needed, generate a random vector for starting the arnoldi method */
  if (user == 0) {
    CHKERRQ(BVSetRandomColumn(d->eps->V,l));
    user = 1;
  }

  /* Perform k steps of Arnoldi with the operator K^{-1}*(t[1]*A-t[2]*B) */
  CHKERRQ(dvd_orthV(d->eps->V,l,l+user));
  for (i=l+user;i<l+data->k && i<d->eps->ncv && i-l<d->eps->mpd;i++) {
    /* aux <- theta[1]A*in - theta[0]*B*in */
    CHKERRQ(BVGetColumn(d->eps->V,i,&v1));
    CHKERRQ(BVGetColumn(d->eps->V,i-user,&v2));
    CHKERRQ(BVGetColumn(d->auxBV,0,&av));
    if (d->B) {
      CHKERRQ(MatMult(d->A,v2,v1));
      CHKERRQ(MatMult(d->B,v2,av));
      CHKERRQ(VecAXPBY(av,d->target[1],-d->target[0],v1));
    } else {
      CHKERRQ(MatMult(d->A,v2,av));
      CHKERRQ(VecAXPBY(av,-d->target[0],d->target[1],v2));
    }
    CHKERRQ(d->improvex_precond(d,0,av,v1));
    CHKERRQ(BVRestoreColumn(d->eps->V,i,&v1));
    CHKERRQ(BVRestoreColumn(d->eps->V,i-user,&v2));
    CHKERRQ(BVRestoreColumn(d->auxBV,0,&av));
    CHKERRQ(dvd_orthV(d->eps->V,i,i+1));
  }

  d->V_tra_s = 0; d->V_tra_e = 0;
  d->V_new_s = 0; d->V_new_e = i-l;

  /* After that the user vectors will be destroyed */
  data->user = 0;
  PetscFunctionReturn(0);
}

static PetscErrorCode dvd_initV_d(dvdDashboard *d)
{
  dvdInitV       *data = (dvdInitV*)d->initV_data;

  PetscFunctionBegin;
  /* Restore changes in dvdDashboard */
  d->initV_data = data->old_initV_data;

  /* Free local data */
  CHKERRQ(PetscFree(data));
  PetscFunctionReturn(0);
}

PetscErrorCode dvd_initV(dvdDashboard *d, dvdBlackboard *b, PetscInt k,PetscInt user, PetscBool krylov)
{
  dvdInitV       *data;

  PetscFunctionBegin;
  /* Setting configuration constrains */
  b->max_size_V = PetscMax(b->max_size_V, k);

  /* Setup the step */
  if (b->state >= DVD_STATE_CONF) {
    CHKERRQ(PetscNewLog(d->eps,&data));
    data->k = k;
    data->user = user;
    data->old_initV_data = d->initV_data;
    d->initV_data = data;
    if (krylov) d->initV = dvd_initV_krylov_0;
    else d->initV = dvd_initV_classic_0;
    CHKERRQ(EPSDavidsonFLAdd(&d->destroyList,dvd_initV_d));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode dvd_orthV(BV V,PetscInt V_new_s,PetscInt V_new_e)
{
  PetscInt       i;

  PetscFunctionBegin;
  for (i=V_new_s;i<V_new_e;i++) CHKERRQ(BVOrthonormalizeColumn(V,i,PETSC_TRUE,NULL,NULL));
  PetscFunctionReturn(0);
}
