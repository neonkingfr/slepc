/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Routines to set ST methods and options
*/

#include <slepc/private/stimpl.h>      /*I "slepcst.h" I*/

PetscBool         STRegisterAllCalled = PETSC_FALSE;
PetscFunctionList STList = 0;

/*@C
   STSetType - Builds ST for a particular spectral transformation.

   Logically Collective on st

   Input Parameters:
+  st   - the spectral transformation context.
-  type - a known type

   Options Database Key:
.  -st_type <type> - Sets ST type

   Use -help for a list of available transformations

   Notes:
   See "slepc/include/slepcst.h" for available transformations

   Normally, it is best to use the EPSSetFromOptions() command and
   then set the ST type from the options database rather than by using
   this routine.  Using the options database provides the user with
   maximum flexibility in evaluating the many different transformations.

   Level: beginner

.seealso: EPSSetType()

@*/
PetscErrorCode STSetType(ST st,STType type)
{
  PetscErrorCode (*r)(ST);
  PetscBool      match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidCharPointer(type,2);

  CHKERRQ(PetscObjectTypeCompare((PetscObject)st,type,&match));
  if (match) PetscFunctionReturn(0);
  STCheckNotSeized(st,1);

  CHKERRQ(PetscFunctionListFind(STList,type,&r));
  PetscCheck(r,PetscObjectComm((PetscObject)st),PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested ST type %s",type);

  if (st->ops->destroy) CHKERRQ((*st->ops->destroy)(st));
  CHKERRQ(PetscMemzero(st->ops,sizeof(struct _STOps)));

  st->state   = ST_STATE_INITIAL;
  st->opready = PETSC_FALSE;
  CHKERRQ(PetscObjectChangeTypeName((PetscObject)st,type));
  CHKERRQ((*r)(st));
  PetscFunctionReturn(0);
}

/*@C
   STGetType - Gets the ST type name (as a string) from the ST context.

   Not Collective

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  type - name of the spectral transformation

   Level: intermediate

.seealso: STSetType()

@*/
PetscErrorCode STGetType(ST st,STType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidPointer(type,2);
  *type = ((PetscObject)st)->type_name;
  PetscFunctionReturn(0);
}

/*@
   STSetFromOptions - Sets ST options from the options database.
   This routine must be called before STSetUp() if the user is to be
   allowed to set the type of transformation.

   Collective on st

   Input Parameter:
.  st - the spectral transformation context

   Level: beginner

.seealso: STSetOptionsPrefix()
@*/
PetscErrorCode STSetFromOptions(ST st)
{
  PetscErrorCode ierr;
  PetscScalar    s;
  char           type[256];
  PetscBool      flg,bval;
  STMatMode      mode;
  MatStructure   mstr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  CHKERRQ(STRegisterAll());
  ierr = PetscObjectOptionsBegin((PetscObject)st);CHKERRQ(ierr);
    CHKERRQ(PetscOptionsFList("-st_type","Spectral transformation","STSetType",STList,(char*)(((PetscObject)st)->type_name?((PetscObject)st)->type_name:STSHIFT),type,sizeof(type),&flg));
    if (flg) {
      CHKERRQ(STSetType(st,type));
    } else if (!((PetscObject)st)->type_name) {
      CHKERRQ(STSetType(st,STSHIFT));
    }

    CHKERRQ(PetscOptionsScalar("-st_shift","Value of the shift","STSetShift",st->sigma,&s,&flg));
    if (flg) CHKERRQ(STSetShift(st,s));

    CHKERRQ(PetscOptionsEnum("-st_matmode","Matrix mode for transformed matrices","STSetMatMode",STMatModes,(PetscEnum)st->matmode,(PetscEnum*)&mode,&flg));
    if (flg) CHKERRQ(STSetMatMode(st,mode));

    CHKERRQ(PetscOptionsEnum("-st_matstructure","Relation of the sparsity pattern of the matrices","STSetMatStructure",MatStructures,(PetscEnum)st->str,(PetscEnum*)&mstr,&flg));
    if (flg) CHKERRQ(STSetMatStructure(st,mstr));

    CHKERRQ(PetscOptionsBool("-st_transform","Whether transformed matrices are computed or not","STSetTransform",st->transform,&bval,&flg));
    if (flg) CHKERRQ(STSetTransform(st,bval));

    if (st->ops->setfromoptions) {
      CHKERRQ((*st->ops->setfromoptions)(PetscOptionsObject,st));
    }
    CHKERRQ(PetscObjectProcessOptionsHandlers(PetscOptionsObject,(PetscObject)st));
  ierr = PetscOptionsEnd();CHKERRQ(ierr);

  if (st->usesksp) {
    CHKERRQ(STSetDefaultKSP(st));
    CHKERRQ(KSPSetFromOptions(st->ksp));
  }
  PetscFunctionReturn(0);
}

/*@
   STSetMatStructure - Sets an internal MatStructure attribute to
   indicate which is the relation of the sparsity pattern of all ST matrices.

   Logically Collective on st

   Input Parameters:
+  st  - the spectral transformation context
-  str - either SAME_NONZERO_PATTERN, DIFFERENT_NONZERO_PATTERN,
         SUBSET_NONZERO_PATTERN, or UNKNOWN_NONZERO_PATTERN

   Options Database Key:
.  -st_matstructure <str> - Indicates the structure flag, where <str> is one
         of 'same' (matrices have the same nonzero pattern), 'different'
         (different nonzero pattern), 'subset' (pattern is a subset of the
         first one), or 'unknown'.

   Notes:
   If the sparsity pattern of the second matrix is equal or a subset of the
   pattern of the first matrix then it is recommended to set this attribute
   for efficiency reasons (in particular, for internal MatAXPY() operations).
   If not set, the default is UNKNOWN_NONZERO_PATTERN, in which case the patterns
   will be compared to determine if they are equal.

   This function has no effect in the case of standard eigenproblems.

   In case of polynomial eigenproblems, the flag applies to all matrices
   relative to the first one.

   Level: advanced

.seealso: STSetMatrices(), MatAXPY()
@*/
PetscErrorCode STSetMatStructure(ST st,MatStructure str)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidLogicalCollectiveEnum(st,str,2);
  switch (str) {
    case SAME_NONZERO_PATTERN:
    case DIFFERENT_NONZERO_PATTERN:
    case SUBSET_NONZERO_PATTERN:
    case UNKNOWN_NONZERO_PATTERN:
      st->str = str;
      break;
    default:
      SETERRQ(PetscObjectComm((PetscObject)st),PETSC_ERR_ARG_OUTOFRANGE,"Invalid matrix structure flag");
  }
  PetscFunctionReturn(0);
}

/*@
   STGetMatStructure - Gets the internal MatStructure attribute to
   indicate which is the relation of the sparsity pattern of the matrices.

   Not Collective

   Input Parameters:
.  st  - the spectral transformation context

   Output Parameters:
.  str - either SAME_NONZERO_PATTERN, DIFFERENT_NONZERO_PATTERN,
         SUBSET_NONZERO_PATTERN, or UNKNOWN_NONZERO_PATTERN

   Level: advanced

.seealso: STSetMatStructure(), STSetMatrices(), MatAXPY()
@*/
PetscErrorCode STGetMatStructure(ST st,MatStructure *str)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidPointer(str,2);
  *str = st->str;
  PetscFunctionReturn(0);
}

/*@
   STSetMatMode - Sets a flag to indicate how the transformed matrices are
   being stored in the spectral transformations.

   Logically Collective on st

   Input Parameters:
+  st - the spectral transformation context
-  mode - the mode flag, one of ST_MATMODE_COPY,
          ST_MATMODE_INPLACE, or ST_MATMODE_SHELL

   Options Database Key:
.  -st_matmode <mode> - Indicates the mode flag, where <mode> is one of
          'copy', 'inplace', 'shell' (see explanation below).

   Notes:
   By default (ST_MATMODE_COPY), a copy of matrix A is made and then
   this copy is modified explicitly, e.g. A <- (A - s B).

   With ST_MATMODE_INPLACE, the original matrix A is modified at STSetUp()
   and changes are reverted at the end of the computations. With respect to
   the previous one, this mode avoids a copy of matrix A. However, a
   drawback is that the recovered matrix might be slightly different
   from the original one (due to roundoff).

   With ST_MATMODE_SHELL, the solver works with an implicit shell
   matrix that represents the shifted matrix. This mode is the most efficient
   in creating the shifted matrix but it places serious limitations to the
   linear solves performed in each iteration of the eigensolver (typically,
   only iterative solvers with Jacobi preconditioning can be used).

   In the two first modes the efficiency of the computation
   can be controlled with STSetMatStructure().

   Level: intermediate

.seealso: STSetMatrices(), STSetMatStructure(), STGetMatMode(), STMatMode
@*/
PetscErrorCode STSetMatMode(ST st,STMatMode mode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidLogicalCollectiveEnum(st,mode,2);
  if (st->matmode != mode) {
    STCheckNotSeized(st,1);
    st->matmode = mode;
    st->state   = ST_STATE_INITIAL;
    st->opready = PETSC_FALSE;
  }
  PetscFunctionReturn(0);
}

/*@
   STGetMatMode - Gets a flag that indicates how the transformed matrices
   are stored in spectral transformations.

   Not Collective

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  mode - the mode flag

   Level: intermediate

.seealso: STSetMatMode(), STMatMode
@*/
PetscErrorCode STGetMatMode(ST st,STMatMode *mode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidPointer(mode,2);
  *mode = st->matmode;
  PetscFunctionReturn(0);
}

/*@
   STSetTransform - Sets a flag to indicate whether the transformed matrices are
   computed or not.

   Logically Collective on st

   Input Parameters:
+  st  - the spectral transformation context
-  flg - the boolean flag

   Options Database Key:
.  -st_transform <bool> - Activate/deactivate the computation of matrices.

   Notes:
   This flag is intended for the case of polynomial eigenproblems solved
   via linearization. If this flag is off (default) the spectral transformation
   is applied to the linearization (handled by the eigensolver), otherwise
   it is applied to the original problem.

   Level: developer

.seealso: STMatSolve(), STMatMult(), STSetMatStructure(), STGetTransform()
@*/
PetscErrorCode STSetTransform(ST st,PetscBool flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidLogicalCollectiveBool(st,flg,2);
  if (st->transform != flg) {
    st->transform = flg;
    st->state     = ST_STATE_INITIAL;
    st->opready   = PETSC_FALSE;
  }
  PetscFunctionReturn(0);
}

/*@
   STGetTransform - Gets a flag that that indicates whether the transformed
   matrices are computed or not.

   Not Collective

   Input Parameter:
.  st - the spectral transformation context

   Output Parameter:
.  flg - the flag

   Level: developer

.seealso: STSetTransform()
@*/
PetscErrorCode STGetTransform(ST st,PetscBool *flg)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(st,ST_CLASSID,1);
  PetscValidBoolPointer(flg,2);
  *flg = st->transform;
  PetscFunctionReturn(0);
}
