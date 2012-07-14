/*
   Basic PS routines

   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2011, Universitat Politecnica de Valencia, Spain

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

#include <slepc-private/psimpl.h>      /*I "slepcps.h" I*/
#include <slepcblaslapack.h>

PetscFList       PSList = 0;
PetscBool        PSRegisterAllCalled = PETSC_FALSE;
PetscClassId     PS_CLASSID = 0;
PetscLogEvent    PS_Solve = 0,PS_Vectors = 0,PS_Other = 0;
static PetscBool PSPackageInitialized = PETSC_FALSE;
const char       *PSMatName[PS_NUM_MAT] = {"A","B","C","T","D","Q","Z","X","Y","U","VT","W"};

PetscErrorCode SlepcDenseMatProd(PetscScalar *C, PetscInt _ldC, PetscScalar b, PetscScalar a, const PetscScalar *A, PetscInt _ldA, PetscInt rA, PetscInt cA, PetscBool At, const PetscScalar *B, PetscInt _ldB, PetscInt rB, PetscInt cB, PetscBool Bt);

#undef __FUNCT__  
#define __FUNCT__ "PSFinalizePackage"
/*@C
   PSFinalizePackage - This function destroys everything in the Slepc interface 
   to the PS package. It is called from SlepcFinalize().

   Level: developer

.seealso: SlepcFinalize()
@*/
PetscErrorCode PSFinalizePackage(void) 
{
  PetscFunctionBegin;
  PSPackageInitialized = PETSC_FALSE;
  PSList               = 0;
  PSRegisterAllCalled  = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSInitializePackage"
/*@C
  PSInitializePackage - This function initializes everything in the PS package.
  It is called from PetscDLLibraryRegister() when using dynamic libraries, and
  on the first call to PSCreate() when using static libraries.

  Input Parameter:
  path - The dynamic library path, or PETSC_NULL

  Level: developer

.seealso: SlepcInitialize()
@*/
PetscErrorCode PSInitializePackage(const char *path) 
{
  char             logList[256];
  char             *className;
  PetscBool        opt;
  PetscErrorCode   ierr;

  PetscFunctionBegin;
  if (PSPackageInitialized) PetscFunctionReturn(0);
  PSPackageInitialized = PETSC_TRUE;
  /* Register Classes */
  ierr = PetscClassIdRegister("Projected system",&PS_CLASSID);CHKERRQ(ierr);
  /* Register Constructors */
  ierr = PSRegisterAll(path);CHKERRQ(ierr);
  /* Register Events */
  ierr = PetscLogEventRegister("PSSolve",PS_CLASSID,&PS_Solve);CHKERRQ(ierr);
  ierr = PetscLogEventRegister("PSVectors",PS_CLASSID,&PS_Vectors);CHKERRQ(ierr);
  ierr = PetscLogEventRegister("PSOther",PS_CLASSID,&PS_Other);CHKERRQ(ierr);
  /* Process info exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL,"-info_exclude",logList,256,&opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList,"ps",&className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscInfoDeactivateClass(PS_CLASSID);CHKERRQ(ierr);
    }
  }
  /* Process summary exclusions */
  ierr = PetscOptionsGetString(PETSC_NULL,"-log_summary_exclude",logList,256,&opt);CHKERRQ(ierr);
  if (opt) {
    ierr = PetscStrstr(logList,"ps",&className);CHKERRQ(ierr);
    if (className) {
      ierr = PetscLogEventDeactivateClass(PS_CLASSID);CHKERRQ(ierr);
    }
  }
  ierr = PetscRegisterFinalize(PSFinalizePackage);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSCreate"
/*@C
   PSCreate - Creates a PS context.

   Collective on MPI_Comm

   Input Parameter:
.  comm - MPI communicator

   Output Parameter:
.  newps - location to put the PS context

   Level: beginner

   Note: 
   PS objects are not intended for normal users but only for
   advanced user that for instance implement their own solvers.

.seealso: PSDestroy(), PS
@*/
PetscErrorCode PSCreate(MPI_Comm comm,PS *newps)
{
  PS             ps;
  PetscInt       i;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidPointer(newps,2);
  ierr = SlepcHeaderCreate(ps,_p_PS,struct _PSOps,PS_CLASSID,-1,"PS","Projected System","PS",comm,PSDestroy,PSView);CHKERRQ(ierr);
  *newps       = ps;
  ps->state    = PS_STATE_RAW;
  ps->method   = 0;
  ps->compact  = PETSC_FALSE;
  ps->refined  = PETSC_FALSE;
  ps->extrarow = PETSC_FALSE;
  ps->ld       = 0;
  ps->l        = 0;
  ps->n        = 0;
  ps->m        = 0;
  ps->k        = 0;
  for (i=0;i<PS_NUM_MAT;i++) {
    ps->mat[i]  = PETSC_NULL;
    ps->rmat[i] = PETSC_NULL;
  }
  ps->perm     = PETSC_NULL;
  ps->work     = PETSC_NULL;
  ps->rwork    = PETSC_NULL;
  ps->iwork    = PETSC_NULL;
  ps->lwork    = 0;
  ps->lrwork   = 0;
  ps->liwork   = 0;
  ps->comp_fun = PETSC_NULL;
  ps->comp_ctx = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetOptionsPrefix"
/*@C
   PSSetOptionsPrefix - Sets the prefix used for searching for all 
   PS options in the database.

   Logically Collective on PS

   Input Parameters:
+  ps - the projected system context
-  prefix - the prefix string to prepend to all PS option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Level: advanced

.seealso: PSAppendOptionsPrefix()
@*/
PetscErrorCode PSSetOptionsPrefix(PS ps,const char *prefix)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  ierr = PetscObjectSetOptionsPrefix((PetscObject)ps,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSAppendOptionsPrefix"
/*@C
   PSAppendOptionsPrefix - Appends to the prefix used for searching for all 
   PS options in the database.

   Logically Collective on PS

   Input Parameters:
+  ps - the projected system context
-  prefix - the prefix string to prepend to all PS option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

   Level: advanced

.seealso: PSSetOptionsPrefix()
@*/
PetscErrorCode PSAppendOptionsPrefix(PS ps,const char *prefix)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  ierr = PetscObjectAppendOptionsPrefix((PetscObject)ps,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "PSGetOptionsPrefix"
/*@C
   PSGetOptionsPrefix - Gets the prefix used for searching for all 
   PS options in the database.

   Not Collective

   Input Parameters:
.  ps - the projected system context

   Output Parameters:
.  prefix - pointer to the prefix string used is returned

   Notes: On the fortran side, the user should pass in a string 'prefix' of
   sufficient length to hold the prefix.

   Level: advanced

.seealso: PSSetOptionsPrefix(), PSAppendOptionsPrefix()
@*/
PetscErrorCode PSGetOptionsPrefix(PS ps,const char *prefix[])
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(prefix,2);
  ierr = PetscObjectGetOptionsPrefix((PetscObject)ps,prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetType"
/*@C
   PSSetType - Selects the type for the PS object.

   Logically Collective on PS

   Input Parameter:
+  ps   - the projected system context
-  type - a known type

   Level: advanced

.seealso: PSGetType()
@*/
PetscErrorCode PSSetType(PS ps,const PSType type)
{
  PetscErrorCode ierr,(*r)(PS);
  PetscBool      match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidCharPointer(type,2);

  ierr = PetscObjectTypeCompare((PetscObject)ps,type,&match);CHKERRQ(ierr);
  if (match) PetscFunctionReturn(0);

  ierr =  PetscFListFind(PSList,((PetscObject)ps)->comm,type,PETSC_TRUE,(void (**)(void))&r);CHKERRQ(ierr);
  if (!r) SETERRQ1(((PetscObject)ps)->comm,PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested PS type %s",type);

  ierr = PetscMemzero(ps->ops,sizeof(struct _PSOps));CHKERRQ(ierr);

  ierr = PetscObjectChangeTypeName((PetscObject)ps,type);CHKERRQ(ierr);
  ierr = (*r)(ps);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetType"
/*@C
   PSGetType - Gets the PS type name (as a string) from the PS context.

   Not Collective

   Input Parameter:
.  ps - the projected system context

   Output Parameter:
.  name - name of the projected system

   Level: advanced

.seealso: PSSetType()
@*/
PetscErrorCode PSGetType(PS ps,const PSType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(type,2);
  *type = ((PetscObject)ps)->type_name;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetMethod"
/*@
   PSSetMethod - Selects the method to be used to solve the problem.

   Logically Collective on PS

   Input Parameter:
+  ps   - the projected system context
-  meth - an index indentifying the method

   Level: advanced

.seealso: PSGetMethod()
@*/
PetscErrorCode PSSetMethod(PS ps,PetscInt meth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveInt(ps,meth,2);
  if (meth<0) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"The method must be a non-negative integer");
  if (meth>PS_MAX_SOLVE) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Too large value for the method");
  ps->method = meth;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetMethod"
/*@
   PSGetMethod - Gets the method currently used in the PS.

   Not Collective

   Input Parameter:
.  ps - the projected system context

   Output Parameter:
.  meth - identifier of the method

   Level: advanced

.seealso: PSSetMethod()
@*/
PetscErrorCode PSGetMethod(PS ps,PetscInt *meth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(meth,2);
  *meth = ps->method;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetCompact"
/*@
   PSSetCompact - Switch to compact storage of matrices.

   Logically Collective on PS

   Input Parameter:
+  ps   - the projected system context
-  comp - a boolean flag

   Notes:
   Compact storage is used in some PS types such as PSHEP when the matrix
   is tridiagonal. This flag can be used to indicate whether the user
   provides the matrix entries via the compact form (the tridiagonal PS_MAT_T)
   or the non-compact one (PS_MAT_A).

   The default is PETSC_FALSE.

   Level: advanced

.seealso: PSGetCompact()
@*/
PetscErrorCode PSSetCompact(PS ps,PetscBool comp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveBool(ps,comp,2);
  ps->compact = comp;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetCompact"
/*@
   PSGetCompact - Gets the compact storage flag.

   Not Collective

   Input Parameter:
.  ps - the projected system context

   Output Parameter:
.  comp - the flag

   Level: advanced

.seealso: PSSetCompact()
@*/
PetscErrorCode PSGetCompact(PS ps,PetscBool *comp)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(comp,2);
  *comp = ps->compact;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetExtraRow"
/*@
   PSSetExtraRow - Sets a flag to indicate that the matrix has one extra
   row.

   Logically Collective on PS

   Input Parameter:
+  ps  - the projected system context
-  ext - a boolean flag

   Notes:
   In Krylov methods it is useful that the matrix representing the projected system
   has one extra row, i.e., has dimension (n+1) x n. If this flag is activated, all
   transformations applied to the right of the matrix also affect this additional
   row. In that case, (n+1) must be less or equal than the leading dimension.

   The default is PETSC_FALSE.

   Level: advanced

.seealso: PSSolve(), PSAllocate(), PSGetExtraRow()
@*/
PetscErrorCode PSSetExtraRow(PS ps,PetscBool ext)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveBool(ps,ext,2);
  if (ps->n>0 && ps->n==ps->ld) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ORDER,"Cannot set extra row after setting n=ld");
  ps->extrarow = ext;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetExtraRow"
/*@
   PSGetExtraRow - Gets the extra row flag.

   Not Collective

   Input Parameter:
.  ps - the projected system context

   Output Parameter:
.  ext - the flag

   Level: advanced

.seealso: PSSetExtraRow()
@*/
PetscErrorCode PSGetExtraRow(PS ps,PetscBool *ext)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(ext,2);
  *ext = ps->extrarow;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetRefined"
/*@
   PSSetRefined - Sets a flag to indicate that refined vectors must be
   computed.

   Logically Collective on PS

   Input Parameter:
+  ps  - the projected system context
-  ref - a boolean flag

   Notes:
   Normally the vectors returned in PS_MAT_X are eigenvectors of the
   projected matrix. With this flag activated, PSVectors() will return
   the right singular vector of the smallest singular value of matrix
   \tilde{A}-theta*I, where \tilde{A} is the extended (n+1)xn matrix
   and theta is the Ritz value. This is used in the refined Ritz
   approximation.

   The default is PETSC_FALSE.

   Level: advanced

.seealso: PSVectors(), PSGetRefined()
@*/
PetscErrorCode PSSetRefined(PS ps,PetscBool ref)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveBool(ps,ref,2);
  ps->refined = ref;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetRefined"
/*@
   PSGetRefined - Gets the refined vectors flag.

   Not Collective

   Input Parameter:
.  ps - the projected system context

   Output Parameter:
.  ref - the flag

   Level: advanced

.seealso: PSSetRefined()
@*/
PetscErrorCode PSGetRefined(PS ps,PetscBool *ref)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidPointer(ref,2);
  *ref = ps->refined;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetEigenvalueComparison"
/*@C
   PSSetEigenvalueComparison - Specifies the eigenvalue comparison function
   to be used for sorting the result of PSSolve().

   Logically Collective on PS

   Input Parameters:
+  ps  - the projected system context
.  fun - a pointer to the comparison function
-  ctx - a context pointer (the last parameter to the comparison function)

   Calling Sequence of fun:
$  func(PetscScalar ar,PetscScalar ai,PetscScalar br,PetscScalar bi,PetscInt *res,void *ctx)

+   ar     - real part of the 1st eigenvalue
.   ai     - imaginary part of the 1st eigenvalue
.   br     - real part of the 2nd eigenvalue
.   bi     - imaginary part of the 2nd eigenvalue
.   res    - result of comparison
-   ctx    - optional context, as set by PSSetEigenvalueComparison()

   Note:
   The returning parameter 'res' can be:
+  negative - if the 1st eigenvalue is preferred to the 2st one
.  zero     - if both eigenvalues are equally preferred
-  positive - if the 2st eigenvalue is preferred to the 1st one

   Level: advanced

.seealso: PSSolve()
@*/
PetscErrorCode PSSetEigenvalueComparison(PS ps,PetscErrorCode (*fun)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*),void* ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  ps->comp_fun = fun;
  ps->comp_ctx = ctx;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSGetEigenvalueComparison"
/*@C
   PSGetEigenvalueComparison - Gets the eigenvalue comparison function
   used for sorting the result of PSSolve().

   Not Collective

   Input Parameter:
.  ps  - the projected system context

   Output Parameters:
+  fun - a pointer to the comparison function
-  ctx - a context pointer (the last parameter to the comparison function)

   Calling Sequence of fun:
$  func(PetscScalar ar,PetscScalar ai,PetscScalar br,PetscScalar bi,PetscInt *res,void *ctx)

+   ar     - real part of the 1st eigenvalue
.   ai     - imaginary part of the 1st eigenvalue
.   br     - real part of the 2nd eigenvalue
.   bi     - imaginary part of the 2nd eigenvalue
.   res    - result of comparison
-   ctx    - optional context, as set by PSSetEigenvalueComparison()

   Note:
   The returning parameter 'res' can be:
+  negative - if the 1st eigenvalue is preferred to the 2st one
.  zero     - if both eigenvalues are equally preferred
-  positive - if the 2st eigenvalue is preferred to the 1st one

   Level: advanced

.seealso: PSSolve(), PSSetEigenvalueComparison()
@*/
PetscErrorCode PSGetEigenvalueComparison(PS ps,PetscErrorCode (**fun)(PetscScalar,PetscScalar,PetscScalar,PetscScalar,PetscInt*,void*),void** ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  if (fun) *fun = ps->comp_fun;
  if (ctx) *ctx = ps->comp_ctx;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetFromOptions"
/*@
   PSSetFromOptions - Sets PS options from the options database.

   Collective on PS

   Input Parameters:
.  ps - the projected system context

   Notes:  
   To see all options, run your program with the -help option.

   Level: beginner
@*/
PetscErrorCode PSSetFromOptions(PS ps)
{
  PetscErrorCode ierr;
  PetscInt       meth;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  if (!PSRegisterAllCalled) { ierr = PSRegisterAll(PETSC_NULL);CHKERRQ(ierr); }
  /* Set default type (we do not allow changing it with -ps_type) */
  if (!((PetscObject)ps)->type_name) {
    ierr = PSSetType(ps,PSNHEP);CHKERRQ(ierr);
  }
  ierr = PetscOptionsBegin(((PetscObject)ps)->comm,((PetscObject)ps)->prefix,"Projecte System (PS) Options","PS");CHKERRQ(ierr);
    meth = 0;
    ierr = PetscOptionsInt("-ps_method","Method to be used for the projected system","PSSetMethod",ps->method,&meth,PETSC_NULL);CHKERRQ(ierr);
    ierr = PSSetMethod(ps,meth);CHKERRQ(ierr);
    ierr = PetscObjectProcessOptionsHandlers((PetscObject)ps);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSView"
/*@C
   PSView - Prints the PS data structure.

   Collective on PS

   Input Parameters:
+  ps - the projected system context
-  viewer - optional visualization context

   Note:
   The available visualization contexts include
+     PETSC_VIEWER_STDOUT_SELF - standard output (default)
-     PETSC_VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   The user can open an alternative visualization context with
   PetscViewerASCIIOpen() - output to a specified file.

   Level: beginner

.seealso: PetscViewerASCIIOpen()
@*/
PetscErrorCode PSView(PS ps,PetscViewer viewer)
{
  PetscBool         isascii,issvd;
  const char        *state;
  PetscViewerFormat format;
  PetscErrorCode    ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  if (!viewer) viewer = PETSC_VIEWER_STDOUT_(((PetscObject)ps)->comm);
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_CLASSID,2);
  PetscCheckSameComm(ps,1,viewer,2);
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
    ierr = PetscObjectPrintClassNamePrefixType((PetscObject)ps,viewer,"PS Object");CHKERRQ(ierr);
    if (format == PETSC_VIEWER_ASCII_INFO_DETAIL) {
      switch (ps->state) {
        case PS_STATE_RAW:          state = "raw"; break;
        case PS_STATE_INTERMEDIATE: state = "intermediate"; break;
        case PS_STATE_CONDENSED:    state = "condensed"; break;
        case PS_STATE_SORTED:       state = "sorted"; break;
        default: SETERRQ(((PetscObject)ps)->comm,1,"Wrong value of ps->state");
      }
      ierr = PetscViewerASCIIPrintf(viewer,"  current state: %s\n",state);CHKERRQ(ierr);
      ierr = PetscObjectTypeCompare((PetscObject)ps,PSSVD,&issvd);CHKERRQ(ierr);
      if (issvd) {
        ierr = PetscViewerASCIIPrintf(viewer,"  dimensions: ld=%d, n=%d, m=%d, l=%d, k=%d\n",ps->ld,ps->n,ps->m,ps->l,ps->k);CHKERRQ(ierr);
      } else {
        ierr = PetscViewerASCIIPrintf(viewer,"  dimensions: ld=%d, n=%d, l=%d, k=%d\n",ps->ld,ps->n,ps->l,ps->k);CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIIPrintf(viewer,"  flags: %s %s %s\n",ps->compact?"compact":"",ps->extrarow?"extrarow":"",ps->refined?"refined":"");CHKERRQ(ierr);
    }
    if (ps->ops->view) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = (*ps->ops->view)(ps,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
  } else SETERRQ1(((PetscObject)ps)->comm,1,"Viewer type %s not supported for PS",((PetscObject)viewer)->type_name);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSAllocate"
/*@
   PSAllocate - Allocates memory for internal storage or matrices in PS.

   Logically Collective on PS

   Input Parameters:
+  ps - the projected system context
-  ld - leading dimension (maximum allowed dimension for the matrices, including
        the extra row if present)

   Level: advanced

.seealso: PSGetLeadingDimension(), PSSetDimensions(), PSSetExtraRow()
@*/
PetscErrorCode PSAllocate(PS ps,PetscInt ld)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveInt(ps,ld,2);
  if (ld<1) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ARG_OUTOFRANGE,"Leading dimension should be at least one");
  ps->ld = ld;
  ierr = (*ps->ops->allocate)(ps,ld);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSAllocateMat_Private"
PetscErrorCode PSAllocateMat_Private(PS ps,PSMatType m)
{
  PetscInt       sz;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (m==PS_MAT_T) sz = 3*ps->ld*sizeof(PetscScalar);
  else if (m==PS_MAT_D) sz = ps->ld*sizeof(PetscScalar);
  else sz = ps->ld*ps->ld*sizeof(PetscScalar);
  if (ps->mat[m]) { ierr = PetscFree(ps->mat[m]);CHKERRQ(ierr); }
  else { ierr = PetscLogObjectMemory(ps,sz);CHKERRQ(ierr); }
  ierr = PetscMalloc(sz,&ps->mat[m]);CHKERRQ(ierr); 
  ierr = PetscMemzero(ps->mat[m],sz);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSAllocateMatReal_Private"
PetscErrorCode PSAllocateMatReal_Private(PS ps,PSMatType m)
{
  PetscInt       sz;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (m==PS_MAT_T) sz = 3*ps->ld*sizeof(PetscReal);
  else if (m==PS_MAT_D) sz = ps->ld*sizeof(PetscReal);
  else sz = ps->ld*ps->ld*sizeof(PetscReal);
  if (!ps->rmat[m]) {
    ierr = PetscLogObjectMemory(ps,sz);CHKERRQ(ierr);
    ierr = PetscMalloc(sz,&ps->rmat[m]);CHKERRQ(ierr); 
  }
  ierr = PetscMemzero(ps->rmat[m],sz);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSAllocateWork_Private"
PetscErrorCode PSAllocateWork_Private(PS ps,PetscInt s,PetscInt r,PetscInt i)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (s>ps->lwork) {
    ierr = PetscFree(ps->work);CHKERRQ(ierr);
    ierr = PetscMalloc(s*sizeof(PetscScalar),&ps->work);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ps,(s-ps->lwork)*sizeof(PetscScalar));CHKERRQ(ierr); 
    ps->lwork = s;
  }
  if (r>ps->lrwork) {
    ierr = PetscFree(ps->rwork);CHKERRQ(ierr);
    ierr = PetscMalloc(r*sizeof(PetscReal),&ps->rwork);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ps,(r-ps->lrwork)*sizeof(PetscReal));CHKERRQ(ierr); 
    ps->lrwork = r;
  }
  if (i>ps->liwork) {
    ierr = PetscFree(ps->iwork);CHKERRQ(ierr);
    ierr = PetscMalloc(i*sizeof(PetscBLASInt),&ps->iwork);CHKERRQ(ierr);
    ierr = PetscLogObjectMemory(ps,(i-ps->liwork)*sizeof(PetscBLASInt));CHKERRQ(ierr); 
    ps->liwork = i;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSViewMat_Private"
PetscErrorCode PSViewMat_Private(PS ps,PetscViewer viewer,PSMatType m)
{
  PetscErrorCode    ierr;
  PetscInt          i,j,rows,cols;
  PetscScalar       *v;
  PetscViewerFormat format;
#if defined(PETSC_USE_COMPLEX)
  PetscBool         allreal = PETSC_TRUE;
#endif

  PetscFunctionBegin;
  ierr = PetscViewerGetFormat(viewer,&format);CHKERRQ(ierr);
  if (format == PETSC_VIEWER_ASCII_INFO || format == PETSC_VIEWER_ASCII_INFO_DETAIL) PetscFunctionReturn(0);
  ierr = PetscViewerASCIIUseTabs(viewer,PETSC_FALSE);CHKERRQ(ierr);
  rows = (m==PS_MAT_A && ps->extrarow)? ps->n+1: ps->n;
  cols = (ps->m!=0)? ps->m: ps->n;
#if defined(PETSC_USE_COMPLEX)
  /* determine if matrix has all real values */
  v = ps->mat[m];
  for (i=0;i<rows;i++)
    for (j=0;j<cols;j++)
      if (PetscImaginaryPart(v[i+j*ps->ld])) { allreal = PETSC_FALSE; break; }
#endif
  if (format == PETSC_VIEWER_ASCII_MATLAB) {
    ierr = PetscViewerASCIIPrintf(viewer,"%% Size = %D %D\n",rows,cols);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(viewer,"%s = [\n",PSMatName[m]);CHKERRQ(ierr);
  } else {
    ierr = PetscViewerASCIIPrintf(viewer,"Matrix %s =\n",PSMatName[m]);CHKERRQ(ierr);
  }

  for (i=0;i<rows;i++) {
    v = ps->mat[m]+i;
    for (j=0;j<cols;j++) {
#if defined(PETSC_USE_COMPLEX)
      if (allreal) {
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e ",PetscRealPart(*v));CHKERRQ(ierr);
      } else {
        ierr = PetscViewerASCIIPrintf(viewer,"%18.16e + %18.16ei ",PetscRealPart(*v),PetscImaginaryPart(*v));CHKERRQ(ierr);
      }
#else
      ierr = PetscViewerASCIIPrintf(viewer,"%18.16e ",*v);CHKERRQ(ierr);
#endif
      v += ps->ld;
    }
    ierr = PetscViewerASCIIPrintf(viewer,"\n");CHKERRQ(ierr);
  }

  if (format == PETSC_VIEWER_ASCII_MATLAB) {
    ierr = PetscViewerASCIIPrintf(viewer,"];\n");CHKERRQ(ierr);
  }
  ierr = PetscViewerASCIIUseTabs(viewer,PETSC_TRUE);CHKERRQ(ierr);
  ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSortEigenvaluesReal_Private"
PetscErrorCode PSSortEigenvaluesReal_Private(PS ps,PetscInt l,PetscInt n,PetscReal *eig,PetscInt *perm)
{
  PetscErrorCode ierr;
  PetscScalar    re;
  PetscInt       i,j,result,tmp;

  PetscFunctionBegin;
  for (i=0;i<n;i++) perm[i] = i;
  /* insertion sort */
  for (i=l+1;i<n;i++) {
    re = eig[perm[i]];
    j = i-1;
    ierr = (*ps->comp_fun)(re,0.0,eig[perm[j]],0.0,&result,ps->comp_ctx);CHKERRQ(ierr);
    while (result<=0 && j>=l) {
      tmp = perm[j]; perm[j] = perm[j+1]; perm[j+1] = tmp; j--;
      if (j>=0) {
        ierr = (*ps->comp_fun)(re,0.0,eig[perm[j]],0.0,&result,ps->comp_ctx);CHKERRQ(ierr);
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSCopyMatrix_Private"
/*
  PSCopyMatrix_Private - Copies the trailing block of a matrix (from
  rows/columns l to n).
*/
PetscErrorCode PSCopyMatrix_Private(PS ps,PSMatType dst,PSMatType src)
{
  PetscErrorCode ierr;
  PetscInt    j,m,off,ld;
  PetscScalar *S,*D;

  PetscFunctionBegin;
  ld  = ps->ld;
  m   = ps->n-ps->l;
  off = ps->l+ps->l*ld;
  S   = ps->mat[src];
  D   = ps->mat[dst];
  for (j=0;j<m;j++) {
    ierr = PetscMemcpy(D+off+j*ld,S+off+j*ld,m*sizeof(PetscScalar));CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSPermuteColumns_Private"
PetscErrorCode PSPermuteColumns_Private(PS ps,PetscInt l,PetscInt n,PSMatType m,PetscInt *perm)
{
  PetscInt    i,j,k,p,ld;
  PetscScalar *Q,rtmp;

  PetscFunctionBegin;
  ld = ps->ld;
  Q  = ps->mat[PS_MAT_Q];
  for (i=l;i<n;i++) {
    p = perm[i];
    if (p != i) {
      j = i + 1;
      while (perm[j] != i) j++;
      perm[j] = p; perm[i] = i;
      /* swap eigenvectors i and j */
      for (k=0;k<n;k++) {
        rtmp = Q[k+p*ld]; Q[k+p*ld] = Q[k+i*ld]; Q[k+i*ld] = rtmp;
      }
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSReset"
/*@
   PSReset - Resets the PS context to the initial state.

   Collective on PS

   Input Parameter:
.  ps - the projected system context

   Level: advanced

.seealso: PSDestroy()
@*/
PetscErrorCode PSReset(PS ps)
{
  PetscInt       i;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  ps->state    = PS_STATE_RAW;
  ps->method   = 0;
  ps->compact  = PETSC_FALSE;
  ps->refined  = PETSC_FALSE;
  ps->extrarow = PETSC_FALSE;
  ps->ld       = 0;
  ps->l        = 0;
  ps->n        = 0;
  ps->m        = 0;
  ps->k        = 0;
  for (i=0;i<PS_NUM_MAT;i++) {
    ierr = PetscFree(ps->mat[i]);CHKERRQ(ierr);
    ierr = PetscFree(ps->rmat[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(ps->perm);CHKERRQ(ierr);
  ierr = PetscFree(ps->work);CHKERRQ(ierr);
  ierr = PetscFree(ps->rwork);CHKERRQ(ierr);
  ierr = PetscFree(ps->iwork);CHKERRQ(ierr);
  ps->lwork    = 0;
  ps->lrwork   = 0;
  ps->liwork   = 0;
  ps->comp_fun = PETSC_NULL;
  ps->comp_ctx = PETSC_NULL;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSDestroy"
/*@C
   PSDestroy - Destroys PS context that was created with PSCreate().

   Collective on PS

   Input Parameter:
.  ps - the projected system context

   Level: beginner

.seealso: PSCreate()
@*/
PetscErrorCode PSDestroy(PS *ps)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!*ps) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(*ps,PS_CLASSID,1);
  if (--((PetscObject)(*ps))->refct > 0) { *ps = 0; PetscFunctionReturn(0); }
  ierr = PSReset(*ps);CHKERRQ(ierr);
  ierr = PetscHeaderDestroy(ps);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSRegister"
/*@C
   PSRegister - See PSRegisterDynamic()

   Level: advanced
@*/
PetscErrorCode PSRegister(const char *sname,const char *path,const char *name,PetscErrorCode (*function)(PS))
{
  PetscErrorCode ierr;
  char           fullname[PETSC_MAX_PATH_LEN];

  PetscFunctionBegin;
  ierr = PetscFListConcat(path,name,fullname);CHKERRQ(ierr);
  ierr = PetscFListAdd(&PSList,sname,fullname,(void (*)(void))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSRegisterDestroy"
/*@
   PSRegisterDestroy - Frees the list of PS methods that were
   registered by PSRegisterDynamic().

   Not Collective

   Level: advanced

.seealso: PSRegisterDynamic(), PSRegisterAll()
@*/
PetscErrorCode PSRegisterDestroy(void)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFListDestroy(&PSList);CHKERRQ(ierr);
  PSRegisterAllCalled = PETSC_FALSE;
  PetscFunctionReturn(0);
}

EXTERN_C_BEGIN
extern PetscErrorCode PSCreate_HEP(PS);
extern PetscErrorCode PSCreate_NHEP(PS);
extern PetscErrorCode PSCreate_GHIEP(PS);
extern PetscErrorCode PSCreate_GNHEP(PS);
extern PetscErrorCode PSCreate_SVD(PS);
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "PSRegisterAll"
/*@C
   PSRegisterAll - Registers all of the projected systems in the PS package.

   Not Collective

   Input Parameter:
.  path - the library where the routines are to be found (optional)

   Level: advanced
@*/
PetscErrorCode PSRegisterAll(const char *path)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PSRegisterAllCalled = PETSC_TRUE;
  ierr = PSRegisterDynamic(PSHEP,path,"PSCreate_HEP",PSCreate_HEP);CHKERRQ(ierr);
  ierr = PSRegisterDynamic(PSNHEP,path,"PSCreate_NHEP",PSCreate_NHEP);CHKERRQ(ierr);
  ierr = PSRegisterDynamic(PSGHIEP,path,"PSCreate_GHIEP",PSCreate_GHIEP);CHKERRQ(ierr);
  ierr = PSRegisterDynamic(PSGNHEP,path,"PSCreate_GNHEP",PSCreate_GNHEP);CHKERRQ(ierr);
  ierr = PSRegisterDynamic(PSSVD,path,"PSCreate_SVD",PSCreate_SVD);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSSetIdentity"
/*@
   PSSetIdentity - Copy the identity (a diagonal matrix with ones) on the
   active part of a matrix.

   Logically Collective on PS

   Input Parameters:
+  ps  - the projected system context
-  mat - a matrix

   Level: advanced
@*/
PetscErrorCode PSSetIdentity(PS ps,PSMatType mat)
{
  PetscErrorCode ierr;
  PetscScalar    *x;
  PetscInt       i,ld,n,l;

  PetscFunctionBegin;
  ierr = PSGetDimensions(ps,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = PSGetLeadingDimension(ps,&ld);CHKERRQ(ierr);
  ierr = PSGetArray(ps,mat,&x);CHKERRQ(ierr);
  ierr = PetscLogEventBegin(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  ierr = PetscMemzero(&x[ld*l],ld*(n-l)*sizeof(PetscScalar));
  for (i=l; i<n; i++) {
    x[ld*i+i] = 1.0;
  }
  ierr = PSRestoreArray(ps,mat,&x);CHKERRQ(ierr);
  ierr = PetscLogEventEnd(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "PSOrthogonalize"
/*@
   PSOrthogonalize - Orthogonalize the columns of a matrix.

   Logically Collective on PS

   Input Parameters:
+  ps   - the projected system context
.  mat  - a matrix
-  cols - number of columns to orthogonalize (starting from the column zero)

   Output Parameter:
.  lindcols - number of linearly independent columns of the matrix (can be PETSC_NULL) 

   Level: advanced
@*/
PetscErrorCode PSOrthogonalize(PS ps,PSMatType mat,PetscInt cols,PetscInt *lindcols)
{
#if defined(PETSC_MISSING_LAPACK_GEQRF) || defined(SLEPC_MISSING_LAPACK_ORGQR)
  PetscFunctionBegin;
  SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"GEQRF/ORGQR - Lapack routine is unavailable.");
#else
  PetscErrorCode  ierr;
  PetscInt        n,l,ld;
  PetscBLASInt    ld_,rA,cA,info,ltau,lw;
  PetscScalar     *A,*tau,*w,saux;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveEnum(ps,mat,2);
  PetscValidLogicalCollectiveInt(ps,cols,3);
  ierr = PSGetDimensions(ps,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = PSGetLeadingDimension(ps,&ld);CHKERRQ(ierr);
  n = n - l;
  if (cols > n) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ARG_WRONG,"Invalid number of columns");
  if (n == 0 || cols == 0) { PetscFunctionReturn(0); }
  ierr = PSGetArray(ps,mat,&A);CHKERRQ(ierr);
  ltau = PetscBLASIntCast(PetscMin(cols,n));
  ld_ = PetscBLASIntCast(ld);
  rA = PetscBLASIntCast(n);
  cA = PetscBLASIntCast(cols);
  lw = -1;
  LAPACKgeqrf_(&rA,&cA,A,&ld_,PETSC_NULL,&saux,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGEQRF %d",info);
  lw = (PetscBLASInt)saux;
  ierr = PSAllocateWork_Private(ps,lw+ltau,0,0);CHKERRQ(ierr);
  tau = ps->work;
  w = &tau[ltau];
  ierr = PetscLogEventBegin(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  ierr = PetscFPTrapPush(PETSC_FP_TRAP_OFF);CHKERRQ(ierr);
  LAPACKgeqrf_(&rA,&cA,&A[ld*l+l],&ld_,tau,w,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xGEQRF %d",info);
  LAPACKorgqr_(&rA,&ltau,&ltau,&A[ld*l+l],&ld,tau,w,&lw,&info);
  if (info) SETERRQ1(PETSC_COMM_SELF,PETSC_ERR_LIB,"Error in Lapack xORGQR %d",info);
  ierr = PetscFPTrapPop();CHKERRQ(ierr);
  ierr = PetscLogEventEnd(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  ierr = PSRestoreArray(ps,mat,&A);CHKERRQ(ierr);
  ierr = PetscObjectStateIncrease((PetscObject)ps);CHKERRQ(ierr);
  if (lindcols) *lindcols = ltau;
  PetscFunctionReturn(0);
#endif
}

#undef __FUNCT__  
#define __FUNCT__ "PSPseudoOrthogonalize"
/*@
   PSPseudoOrthogonalize - Orthogonalize the columns of a matrix with Modified
   Gram-Schmidt in an indefinite inner product space defined by a signature.

   Logically Collective on PS

   Input Parameters:
+  ps   - the projected system context
.  mat  - the matrix
.  cols - number of columns to orthogonalize (starting from the column zero)
-  s    - the signature that defines the inner product

   Output Parameter:
+  lindcols - linear independent columns of the matrix (can be PETSC_NULL) 
-  ns - the new norm of the vectors (can be PETSC_NULL)

   Level: advanced
@*/
PetscErrorCode PSPseudoOrthogonalize(PS ps,PSMatType mat,PetscInt cols,PetscReal *s,PetscInt *lindcols,PetscReal *ns)
{
  PetscErrorCode  ierr;
  PetscInt        i,j,k,l,n,ld;
  PetscBLASInt    one=1,rA_;
  PetscScalar     alpha,*A,*A_,*m,*h,nr0;
  PetscReal       nr_o,nr,*ns_;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(ps,PS_CLASSID,1);
  PetscValidLogicalCollectiveEnum(ps,mat,2);
  PetscValidLogicalCollectiveInt(ps,cols,3);
  PetscValidScalarPointer(s,4);
  if (ns) PetscValidRealPointer(ns,6);
  ierr = PSGetDimensions(ps,&n,PETSC_NULL,&l,PETSC_NULL);CHKERRQ(ierr);
  ierr = PSGetLeadingDimension(ps,&ld);CHKERRQ(ierr);
  n = n - l;
  if (cols > n) SETERRQ(((PetscObject)ps)->comm,PETSC_ERR_ARG_WRONG,"Invalid number of columns");
  if (n == 0 || cols == 0) { PetscFunctionReturn(0); }
  rA_ = PetscBLASIntCast(n);
  ierr = PSGetArray(ps,mat,&A_);CHKERRQ(ierr);
  A = &A_[ld*l+l];
  ierr = PSAllocateWork_Private(ps,n+cols,ns?0:cols,0);CHKERRQ(ierr);
  m = ps->work;
  h = &m[n];
  ns_ = ns ? ns : ps->rwork;
  ierr = PetscLogEventBegin(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  for (i=0; i<cols; i++) {
    /* m <- diag(s)*A[i] */
    for (k=0; k<n; k++) m[k] = s[k]*A[k+i*ld];
    /* nr_o <- mynorm(A[i]'*m), mynorm(x) = sign(x)*sqrt(|x|) */
    ierr = SlepcDenseMatProd(&nr0,1,0.0,1.0,&A[ld*i],ld,n,1,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
    nr = nr_o = PetscSign(PetscRealPart(nr0))*PetscSqrtReal(PetscAbsScalar(nr0));
    for (j=0; j<3 && i>0; j++) {
      /* h <- A[0:i-1]'*m */
      ierr = SlepcDenseMatProd(h,i,0.0,1.0,A,ld,n,i,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
      /* h <- diag(ns)*h */
      for (k=0; k<i; k++) h[k] *= ns_[k];
      /* A[i] <- A[i] - A[0:i-1]*h */
      ierr = SlepcDenseMatProd(&A[ld*i],ld,1.0,-1.0,A,ld,n,i,PETSC_FALSE,h,i,i,1,PETSC_FALSE);CHKERRQ(ierr);
      /* m <- diag(s)*A[i] */
      for (k=0; k<n; k++) m[k] = s[k]*A[k+i*ld];
      /* nr_o <- mynorm(A[i]'*m) */
      ierr = SlepcDenseMatProd(&nr0,1,0.0,1.0,&A[ld*i],ld,n,1,PETSC_TRUE,m,n,n,1,PETSC_FALSE);CHKERRQ(ierr);
      nr = PetscSign(PetscRealPart(nr0))*PetscSqrtReal(PetscAbsScalar(nr0));
      if (PetscAbs(nr) < PETSC_MACHINE_EPSILON) SETERRQ(PETSC_COMM_SELF,1, "Linear dependency detected!");
      if (PetscAbs(nr) > 0.7*PetscAbs(nr_o)) break;
      nr_o = nr;
    }
    ns_[i] = PetscSign(nr);
    /* A[i] <- A[i]/|nr| */
    alpha = 1.0/PetscAbs(nr);
    BLASscal_(&rA_,&alpha,&A[i*ld],&one);
  }
  ierr = PetscLogEventEnd(PS_Other,ps,0,0,0);CHKERRQ(ierr);
  ierr = PSRestoreArray(ps,mat,&A_);CHKERRQ(ierr);
  ierr = PetscObjectStateIncrease((PetscObject)ps);CHKERRQ(ierr);
  if (lindcols) *lindcols = cols;
  PetscFunctionReturn(0);
}
