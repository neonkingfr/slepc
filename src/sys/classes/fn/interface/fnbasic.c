/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Basic FN routines
*/

#include <slepc/private/fnimpl.h>      /*I "slepcfn.h" I*/
#include <slepcblaslapack.h>

PetscFunctionList FNList = 0;
PetscBool         FNRegisterAllCalled = PETSC_FALSE;
PetscClassId      FN_CLASSID = 0;
PetscLogEvent     FN_Evaluate = 0;
static PetscBool  FNPackageInitialized = PETSC_FALSE;

const char *FNParallelTypes[] = {"REDUNDANT","SYNCHRONIZED","FNParallelType","FN_PARALLEL_",0};

/*@C
   FNFinalizePackage - This function destroys everything in the Slepc interface
   to the FN package. It is called from SlepcFinalize().

   Level: developer

.seealso: SlepcFinalize()
@*/
PetscErrorCode FNFinalizePackage(void)
{
  PetscFunctionBegin;
  CHKERRQ(PetscFunctionListDestroy(&FNList));
  FNPackageInitialized = PETSC_FALSE;
  FNRegisterAllCalled  = PETSC_FALSE;
  PetscFunctionReturn(0);
}

/*@C
  FNInitializePackage - This function initializes everything in the FN package.
  It is called from PetscDLLibraryRegister() when using dynamic libraries, and
  on the first call to FNCreate() when using static libraries.

  Level: developer

.seealso: SlepcInitialize()
@*/
PetscErrorCode FNInitializePackage(void)
{
  char           logList[256];
  PetscBool      opt,pkg;
  PetscClassId   classids[1];

  PetscFunctionBegin;
  if (FNPackageInitialized) PetscFunctionReturn(0);
  FNPackageInitialized = PETSC_TRUE;
  /* Register Classes */
  CHKERRQ(PetscClassIdRegister("Math Function",&FN_CLASSID));
  /* Register Constructors */
  CHKERRQ(FNRegisterAll());
  /* Register Events */
  CHKERRQ(PetscLogEventRegister("FNEvaluate",FN_CLASSID,&FN_Evaluate));
  /* Process Info */
  classids[0] = FN_CLASSID;
  CHKERRQ(PetscInfoProcessClass("fn",1,&classids[0]));
  /* Process summary exclusions */
  CHKERRQ(PetscOptionsGetString(NULL,NULL,"-log_exclude",logList,sizeof(logList),&opt));
  if (opt) {
    CHKERRQ(PetscStrInList("fn",logList,',',&pkg));
    if (pkg) CHKERRQ(PetscLogEventDeactivateClass(FN_CLASSID));
  }
  /* Register package finalizer */
  CHKERRQ(PetscRegisterFinalize(FNFinalizePackage));
  PetscFunctionReturn(0);
}

/*@
   FNCreate - Creates an FN context.

   Collective

   Input Parameter:
.  comm - MPI communicator

   Output Parameter:
.  newfn - location to put the FN context

   Level: beginner

.seealso: FNDestroy(), FN
@*/
PetscErrorCode FNCreate(MPI_Comm comm,FN *newfn)
{
  FN             fn;

  PetscFunctionBegin;
  PetscValidPointer(newfn,2);
  *newfn = 0;
  CHKERRQ(FNInitializePackage());
  CHKERRQ(SlepcHeaderCreate(fn,FN_CLASSID,"FN","Math Function","FN",comm,FNDestroy,FNView));

  fn->alpha    = 1.0;
  fn->beta     = 1.0;
  fn->method   = 0;

  fn->nw       = 0;
  fn->cw       = 0;
  fn->data     = NULL;

  *newfn = fn;
  PetscFunctionReturn(0);
}

/*@C
   FNSetOptionsPrefix - Sets the prefix used for searching for all
   FN options in the database.

   Logically Collective on fn

   Input Parameters:
+  fn - the math function context
-  prefix - the prefix string to prepend to all FN option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

   Level: advanced

.seealso: FNAppendOptionsPrefix()
@*/
PetscErrorCode FNSetOptionsPrefix(FN fn,const char *prefix)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  CHKERRQ(PetscObjectSetOptionsPrefix((PetscObject)fn,prefix));
  PetscFunctionReturn(0);
}

/*@C
   FNAppendOptionsPrefix - Appends to the prefix used for searching for all
   FN options in the database.

   Logically Collective on fn

   Input Parameters:
+  fn - the math function context
-  prefix - the prefix string to prepend to all FN option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the hyphen.

   Level: advanced

.seealso: FNSetOptionsPrefix()
@*/
PetscErrorCode FNAppendOptionsPrefix(FN fn,const char *prefix)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  CHKERRQ(PetscObjectAppendOptionsPrefix((PetscObject)fn,prefix));
  PetscFunctionReturn(0);
}

/*@C
   FNGetOptionsPrefix - Gets the prefix used for searching for all
   FN options in the database.

   Not Collective

   Input Parameters:
.  fn - the math function context

   Output Parameters:
.  prefix - pointer to the prefix string used is returned

   Note:
   On the Fortran side, the user should pass in a string 'prefix' of
   sufficient length to hold the prefix.

   Level: advanced

.seealso: FNSetOptionsPrefix(), FNAppendOptionsPrefix()
@*/
PetscErrorCode FNGetOptionsPrefix(FN fn,const char *prefix[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidPointer(prefix,2);
  CHKERRQ(PetscObjectGetOptionsPrefix((PetscObject)fn,prefix));
  PetscFunctionReturn(0);
}

/*@C
   FNSetType - Selects the type for the FN object.

   Logically Collective on fn

   Input Parameters:
+  fn   - the math function context
-  type - a known type

   Notes:
   The default is FNRATIONAL, which includes polynomials as a particular
   case as well as simple functions such as f(x)=x and f(x)=constant.

   Level: intermediate

.seealso: FNGetType()
@*/
PetscErrorCode FNSetType(FN fn,FNType type)
{
  PetscErrorCode (*r)(FN);
  PetscBool      match;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidCharPointer(type,2);

  CHKERRQ(PetscObjectTypeCompare((PetscObject)fn,type,&match));
  if (match) PetscFunctionReturn(0);

  CHKERRQ(PetscFunctionListFind(FNList,type,&r));
  PetscCheck(r,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_UNKNOWN_TYPE,"Unable to find requested FN type %s",type);

  if (fn->ops->destroy) CHKERRQ((*fn->ops->destroy)(fn));
  CHKERRQ(PetscMemzero(fn->ops,sizeof(struct _FNOps)));

  CHKERRQ(PetscObjectChangeTypeName((PetscObject)fn,type));
  CHKERRQ((*r)(fn));
  PetscFunctionReturn(0);
}

/*@C
   FNGetType - Gets the FN type name (as a string) from the FN context.

   Not Collective

   Input Parameter:
.  fn - the math function context

   Output Parameter:
.  type - name of the math function

   Level: intermediate

.seealso: FNSetType()
@*/
PetscErrorCode FNGetType(FN fn,FNType *type)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidPointer(type,2);
  *type = ((PetscObject)fn)->type_name;
  PetscFunctionReturn(0);
}

/*@
   FNSetScale - Sets the scaling parameters that define the matematical function.

   Logically Collective on fn

   Input Parameters:
+  fn    - the math function context
.  alpha - inner scaling (argument)
-  beta  - outer scaling (result)

   Notes:
   Given a function f(x) specified by the FN type, the scaling parameters can
   be used to realize the function beta*f(alpha*x). So when these values are given,
   the procedure for function evaluation will first multiply the argument by alpha,
   then evaluate the function itself, and finally scale the result by beta.
   Likewise, these values are also considered when evaluating the derivative.

   If you want to provide only one of the two scaling factors, set the other
   one to 1.0.

   Level: intermediate

.seealso: FNGetScale(), FNEvaluateFunction()
@*/
PetscErrorCode FNSetScale(FN fn,PetscScalar alpha,PetscScalar beta)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidLogicalCollectiveScalar(fn,alpha,2);
  PetscValidLogicalCollectiveScalar(fn,beta,3);
  PetscCheck(PetscAbsScalar(alpha)!=0.0 && PetscAbsScalar(beta)!=0.0,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_WRONG,"Scaling factors must be nonzero");
  fn->alpha = alpha;
  fn->beta  = beta;
  PetscFunctionReturn(0);
}

/*@
   FNGetScale - Gets the scaling parameters that define the matematical function.

   Not Collective

   Input Parameter:
.  fn    - the math function context

   Output Parameters:
+  alpha - inner scaling (argument)
-  beta  - outer scaling (result)

   Level: intermediate

.seealso: FNSetScale()
@*/
PetscErrorCode FNGetScale(FN fn,PetscScalar *alpha,PetscScalar *beta)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  if (alpha) *alpha = fn->alpha;
  if (beta)  *beta  = fn->beta;
  PetscFunctionReturn(0);
}

/*@
   FNSetMethod - Selects the method to be used to evaluate functions of matrices.

   Logically Collective on fn

   Input Parameters:
+  fn   - the math function context
-  meth - an index identifying the method

   Options Database Key:
.  -fn_method <meth> - Sets the method

   Notes:
   In some FN types there are more than one algorithms available for computing
   matrix functions. In that case, this function allows choosing the wanted method.

   If meth is currently set to 0 (the default) and the input argument A of
   FNEvaluateFunctionMat() is a symmetric/Hermitian matrix, then the computation
   is done via the eigendecomposition of A, rather than with the general algorithm.

   Level: intermediate

.seealso: FNGetMethod(), FNEvaluateFunctionMat()
@*/
PetscErrorCode FNSetMethod(FN fn,PetscInt meth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidLogicalCollectiveInt(fn,meth,2);
  PetscCheck(meth>=0,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_OUTOFRANGE,"The method must be a non-negative integer");
  PetscCheck(meth<=FN_MAX_SOLVE,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_OUTOFRANGE,"Too large value for the method");
  fn->method = meth;
  PetscFunctionReturn(0);
}

/*@
   FNGetMethod - Gets the method currently used in the FN.

   Not Collective

   Input Parameter:
.  fn - the math function context

   Output Parameter:
.  meth - identifier of the method

   Level: intermediate

.seealso: FNSetMethod()
@*/
PetscErrorCode FNGetMethod(FN fn,PetscInt *meth)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidIntPointer(meth,2);
  *meth = fn->method;
  PetscFunctionReturn(0);
}

/*@
   FNSetParallel - Selects the mode of operation in parallel runs.

   Logically Collective on fn

   Input Parameters:
+  fn    - the math function context
-  pmode - the parallel mode

   Options Database Key:
.  -fn_parallel <mode> - Sets the parallel mode, either 'redundant' or 'synchronized'

   Notes:
   This is relevant only when the function is evaluated on a matrix, with
   either FNEvaluateFunctionMat() or FNEvaluateFunctionMatVec().

   In the 'redundant' parallel mode, all processes will make the computation
   redundantly, starting from the same data, and producing the same result.
   This result may be slightly different in the different processes if using a
   multithreaded BLAS library, which may cause issues in ill-conditioned problems.

   In the 'synchronized' parallel mode, only the first MPI process performs the
   computation and then the computed matrix is broadcast to the other
   processes in the communicator. This communication is done automatically at
   the end of FNEvaluateFunctionMat() or FNEvaluateFunctionMatVec().

   Level: advanced

.seealso: FNEvaluateFunctionMat() or FNEvaluateFunctionMatVec(), FNGetParallel()
@*/
PetscErrorCode FNSetParallel(FN fn,FNParallelType pmode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidLogicalCollectiveEnum(fn,pmode,2);
  fn->pmode = pmode;
  PetscFunctionReturn(0);
}

/*@
   FNGetParallel - Gets the mode of operation in parallel runs.

   Not Collective

   Input Parameter:
.  fn - the math function context

   Output Parameter:
.  pmode - the parallel mode

   Level: advanced

.seealso: FNSetParallel()
@*/
PetscErrorCode FNGetParallel(FN fn,FNParallelType *pmode)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidPointer(pmode,2);
  *pmode = fn->pmode;
  PetscFunctionReturn(0);
}

/*@
   FNEvaluateFunction - Computes the value of the function f(x) for a given x.

   Not collective

   Input Parameters:
+  fn - the math function context
-  x  - the value where the function must be evaluated

   Output Parameter:
.  y  - the result of f(x)

   Note:
   Scaling factors are taken into account, so the actual function evaluation
   will return beta*f(alpha*x).

   Level: intermediate

.seealso: FNEvaluateDerivative(), FNEvaluateFunctionMat(), FNSetScale()
@*/
PetscErrorCode FNEvaluateFunction(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscScalar    xf,yf;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidType(fn,1);
  PetscValidScalarPointer(y,3);
  CHKERRQ(PetscLogEventBegin(FN_Evaluate,fn,0,0,0));
  xf = fn->alpha*x;
  CHKERRQ((*fn->ops->evaluatefunction)(fn,xf,&yf));
  *y = fn->beta*yf;
  CHKERRQ(PetscLogEventEnd(FN_Evaluate,fn,0,0,0));
  PetscFunctionReturn(0);
}

/*@
   FNEvaluateDerivative - Computes the value of the derivative f'(x) for a given x.

   Not Collective

   Input Parameters:
+  fn - the math function context
-  x  - the value where the derivative must be evaluated

   Output Parameter:
.  y  - the result of f'(x)

   Note:
   Scaling factors are taken into account, so the actual derivative evaluation will
   return alpha*beta*f'(alpha*x).

   Level: intermediate

.seealso: FNEvaluateFunction()
@*/
PetscErrorCode FNEvaluateDerivative(FN fn,PetscScalar x,PetscScalar *y)
{
  PetscScalar    xf,yf;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidType(fn,1);
  PetscValidScalarPointer(y,3);
  CHKERRQ(PetscLogEventBegin(FN_Evaluate,fn,0,0,0));
  xf = fn->alpha*x;
  CHKERRQ((*fn->ops->evaluatederivative)(fn,xf,&yf));
  *y = fn->alpha*fn->beta*yf;
  CHKERRQ(PetscLogEventEnd(FN_Evaluate,fn,0,0,0));
  PetscFunctionReturn(0);
}

static PetscErrorCode FNEvaluateFunctionMat_Sym_Private(FN fn,const PetscScalar *As,PetscScalar *Bs,PetscInt m,PetscBool firstonly)
{
  PetscInt       i,j;
  PetscBLASInt   n,k,ld,lwork,info;
  PetscScalar    *Q,*W,*work,adummy,a,x,y,one=1.0,zero=0.0;
  PetscReal      *eig,dummy;
#if defined(PETSC_USE_COMPLEX)
  PetscReal      *rwork,rdummy;
#endif

  PetscFunctionBegin;
  CHKERRQ(PetscBLASIntCast(m,&n));
  ld = n;
  k = firstonly? 1: n;

  /* workspace query and memory allocation */
  lwork = -1;
#if defined(PETSC_USE_COMPLEX)
  PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","L",&n,&adummy,&ld,&dummy,&a,&lwork,&rdummy,&info));
  CHKERRQ(PetscBLASIntCast((PetscInt)PetscRealPart(a),&lwork));
  CHKERRQ(PetscMalloc5(m,&eig,m*m,&Q,m*k,&W,lwork,&work,PetscMax(1,3*m-2),&rwork));
#else
  PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","L",&n,&adummy,&ld,&dummy,&a,&lwork,&info));
  CHKERRQ(PetscBLASIntCast((PetscInt)a,&lwork));
  CHKERRQ(PetscMalloc4(m,&eig,m*m,&Q,m*k,&W,lwork,&work));
#endif

  /* compute eigendecomposition */
  for (j=0;j<n;j++) for (i=j;i<n;i++) Q[i+j*ld] = As[i+j*ld];
#if defined(PETSC_USE_COMPLEX)
  PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","L",&n,Q,&ld,eig,work,&lwork,rwork,&info));
#else
  PetscStackCallBLAS("LAPACKsyev",LAPACKsyev_("V","L",&n,Q,&ld,eig,work,&lwork,&info));
#endif
  SlepcCheckLapackInfo("syev",info);

  /* W = f(Lambda)*Q' */
  for (i=0;i<n;i++) {
    x = fn->alpha*eig[i];
    CHKERRQ((*fn->ops->evaluatefunction)(fn,x,&y));  /* y = f(x) */
    for (j=0;j<k;j++) W[i+j*ld] = PetscConj(Q[j+i*ld])*fn->beta*y;
  }
  /* Bs = Q*W */
  PetscStackCallBLAS("BLASgemm",BLASgemm_("N","N",&n,&k,&n,&one,Q,&ld,W,&ld,&zero,Bs,&ld));
#if defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscFree5(eig,Q,W,work,rwork));
#else
  CHKERRQ(PetscFree4(eig,Q,W,work));
#endif
  CHKERRQ(PetscLogFlops(9.0*n*n*n+2.0*n*n*n));
  PetscFunctionReturn(0);
}

/*
   FNEvaluateFunctionMat_Sym_Default - given a symmetric matrix A,
   compute the matrix function as f(A)=Q*f(D)*Q' where the spectral
   decomposition of A is A=Q*D*Q'
*/
static PetscErrorCode FNEvaluateFunctionMat_Sym_Default(FN fn,Mat A,Mat B)
{
  PetscInt          m;
  const PetscScalar *As;
  PetscScalar       *Bs;

  PetscFunctionBegin;
  CHKERRQ(MatDenseGetArrayRead(A,&As));
  CHKERRQ(MatDenseGetArray(B,&Bs));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(FNEvaluateFunctionMat_Sym_Private(fn,As,Bs,m,PETSC_FALSE));
  CHKERRQ(MatDenseRestoreArrayRead(A,&As));
  CHKERRQ(MatDenseRestoreArray(B,&Bs));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMat_Private(FN fn,Mat A,Mat B,PetscBool sync)
{
  PetscBool      set,flg,symm=PETSC_FALSE;
  PetscInt       m,n;
  PetscMPIInt    size,rank;
  PetscScalar    *pF;
  Mat            M,F;

  PetscFunctionBegin;
  /* destination matrix */
  F = B?B:A;

  /* check symmetry of A */
  CHKERRQ(MatIsHermitianKnown(A,&set,&flg));
  symm = set? flg: PETSC_FALSE;

  CHKERRMPI(MPI_Comm_size(PetscObjectComm((PetscObject)fn),&size));
  CHKERRMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)fn),&rank));
  if (size==1 || fn->pmode==FN_PARALLEL_REDUNDANT || (fn->pmode==FN_PARALLEL_SYNCHRONIZED && !rank)) {

    CHKERRQ(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
    if (symm && !fn->method) {  /* prefer diagonalization */
      CHKERRQ(PetscInfo(fn,"Computing matrix function via diagonalization\n"));
      CHKERRQ(FNEvaluateFunctionMat_Sym_Default(fn,A,F));
    } else {
      /* scale argument */
      if (fn->alpha!=(PetscScalar)1.0) {
        CHKERRQ(FN_AllocateWorkMat(fn,A,&M));
        CHKERRQ(MatScale(M,fn->alpha));
      } else M = A;
      if (fn->ops->evaluatefunctionmat[fn->method]) CHKERRQ((*fn->ops->evaluatefunctionmat[fn->method])(fn,M,F));
      else {
        PetscCheck(fn->method,PetscObjectComm((PetscObject)fn),PETSC_ERR_SUP,"Matrix functions not implemented in this FN type");
        PetscCheck(!fn->method,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_OUTOFRANGE,"The specified method number does not exist for this FN type");
      }
      if (fn->alpha!=(PetscScalar)1.0) CHKERRQ(FN_FreeWorkMat(fn,&M));
      /* scale result */
      CHKERRQ(MatScale(F,fn->beta));
    }
    CHKERRQ(PetscFPTrapPop());
  }
  if (size>1 && fn->pmode==FN_PARALLEL_SYNCHRONIZED && sync) {  /* synchronize */
    CHKERRQ(MatGetSize(A,&m,&n));
    CHKERRQ(MatDenseGetArray(F,&pF));
    CHKERRMPI(MPI_Bcast(pF,n*n,MPIU_SCALAR,0,PetscObjectComm((PetscObject)fn)));
    CHKERRQ(MatDenseRestoreArray(F,&pF));
  }
  PetscFunctionReturn(0);
}

/*@
   FNEvaluateFunctionMat - Computes the value of the function f(A) for a given
   matrix A, where the result is also a matrix.

   Logically Collective on fn

   Input Parameters:
+  fn - the math function context
-  A  - matrix on which the function must be evaluated

   Output Parameter:
.  B  - (optional) matrix resulting from evaluating f(A)

   Notes:
   Matrix A must be a square sequential dense Mat, with all entries equal on
   all processes (otherwise each process will compute different results).
   If matrix B is provided, it must also be a square sequential dense Mat, and
   both matrices must have the same dimensions. If B is NULL (or B=A) then the
   function will perform an in-place computation, overwriting A with f(A).

   If A is known to be real symmetric or complex Hermitian then it is
   recommended to set the appropriate flag with MatSetOption(), because
   symmetry can sometimes be exploited by the algorithm.

   Scaling factors are taken into account, so the actual function evaluation
   will return beta*f(alpha*A).

   Level: advanced

.seealso: FNEvaluateFunction(), FNEvaluateFunctionMatVec(), FNSetMethod()
@*/
PetscErrorCode FNEvaluateFunctionMat(FN fn,Mat A,Mat B)
{
  PetscBool      inplace=PETSC_FALSE;
  PetscInt       m,n,n1;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidHeaderSpecific(A,MAT_CLASSID,2);
  PetscValidType(fn,1);
  PetscValidType(A,2);
  if (B) {
    PetscValidHeaderSpecific(B,MAT_CLASSID,3);
    PetscValidType(B,3);
  } else inplace = PETSC_TRUE;
  PetscCheckTypeName(A,MATSEQDENSE);
  CHKERRQ(MatGetSize(A,&m,&n));
  PetscCheck(m==n,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_SIZ,"Mat A is not square (has %" PetscInt_FMT " rows, %" PetscInt_FMT " cols)",m,n);
  if (!inplace) {
    PetscCheckTypeName(B,MATSEQDENSE);
    n1 = n;
    CHKERRQ(MatGetSize(B,&m,&n));
    PetscCheck(m==n,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_SIZ,"Mat B is not square (has %" PetscInt_FMT " rows, %" PetscInt_FMT " cols)",m,n);
    PetscCheck(n1==n,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_SIZ,"Matrices A and B must have the same dimension");
  }

  /* evaluate matrix function */
  CHKERRQ(PetscLogEventBegin(FN_Evaluate,fn,0,0,0));
  CHKERRQ(FNEvaluateFunctionMat_Private(fn,A,B,PETSC_TRUE));
  CHKERRQ(PetscLogEventEnd(FN_Evaluate,fn,0,0,0));
  PetscFunctionReturn(0);
}

/*
   FNEvaluateFunctionMatVec_Default - computes the full matrix f(A)
   and then copies the first column.
*/
static PetscErrorCode FNEvaluateFunctionMatVec_Default(FN fn,Mat A,Vec v)
{
  Mat            F;

  PetscFunctionBegin;
  CHKERRQ(FN_AllocateWorkMat(fn,A,&F));
  if (fn->ops->evaluatefunctionmat[fn->method]) CHKERRQ((*fn->ops->evaluatefunctionmat[fn->method])(fn,A,F));
  else {
    PetscCheck(fn->method,PetscObjectComm((PetscObject)fn),PETSC_ERR_SUP,"Matrix functions not implemented in this FN type");
    PetscCheck(!fn->method,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_OUTOFRANGE,"The specified method number does not exist for this FN type");
  }
  CHKERRQ(MatGetColumnVector(F,v,0));
  CHKERRQ(FN_FreeWorkMat(fn,&F));
  PetscFunctionReturn(0);
}

/*
   FNEvaluateFunctionMatVec_Sym_Default - given a symmetric matrix A,
   compute the matrix function as f(A)=Q*f(D)*Q' where the spectral
   decomposition of A is A=Q*D*Q'. Only the first column is computed.
*/
static PetscErrorCode FNEvaluateFunctionMatVec_Sym_Default(FN fn,Mat A,Vec v)
{
  PetscInt          m;
  const PetscScalar *As;
  PetscScalar       *vs;

  PetscFunctionBegin;
  CHKERRQ(MatDenseGetArrayRead(A,&As));
  CHKERRQ(VecGetArray(v,&vs));
  CHKERRQ(MatGetSize(A,&m,NULL));
  CHKERRQ(FNEvaluateFunctionMat_Sym_Private(fn,As,vs,m,PETSC_TRUE));
  CHKERRQ(MatDenseRestoreArrayRead(A,&As));
  CHKERRQ(VecRestoreArray(v,&vs));
  PetscFunctionReturn(0);
}

PetscErrorCode FNEvaluateFunctionMatVec_Private(FN fn,Mat A,Vec v,PetscBool sync)
{
  PetscBool      set,flg,symm=PETSC_FALSE;
  PetscInt       m,n;
  Mat            M;
  PetscMPIInt    size,rank;
  PetscScalar    *pv;

  PetscFunctionBegin;
  /* check symmetry of A */
  CHKERRQ(MatIsHermitianKnown(A,&set,&flg));
  symm = set? flg: PETSC_FALSE;

  /* evaluate matrix function */
  CHKERRMPI(MPI_Comm_size(PetscObjectComm((PetscObject)fn),&size));
  CHKERRMPI(MPI_Comm_rank(PetscObjectComm((PetscObject)fn),&rank));
  if (size==1 || fn->pmode==FN_PARALLEL_REDUNDANT || (fn->pmode==FN_PARALLEL_SYNCHRONIZED && !rank)) {
    CHKERRQ(PetscFPTrapPush(PETSC_FP_TRAP_OFF));
    if (symm && !fn->method) {  /* prefer diagonalization */
      CHKERRQ(PetscInfo(fn,"Computing matrix function via diagonalization\n"));
      CHKERRQ(FNEvaluateFunctionMatVec_Sym_Default(fn,A,v));
    } else {
      /* scale argument */
      if (fn->alpha!=(PetscScalar)1.0) {
        CHKERRQ(FN_AllocateWorkMat(fn,A,&M));
        CHKERRQ(MatScale(M,fn->alpha));
      } else M = A;
      if (fn->ops->evaluatefunctionmatvec[fn->method]) CHKERRQ((*fn->ops->evaluatefunctionmatvec[fn->method])(fn,M,v));
      else CHKERRQ(FNEvaluateFunctionMatVec_Default(fn,M,v));
      if (fn->alpha!=(PetscScalar)1.0) CHKERRQ(FN_FreeWorkMat(fn,&M));
      /* scale result */
      CHKERRQ(VecScale(v,fn->beta));
    }
    CHKERRQ(PetscFPTrapPop());
  }

  /* synchronize */
  if (size>1 && fn->pmode==FN_PARALLEL_SYNCHRONIZED && sync) {
    CHKERRQ(MatGetSize(A,&m,&n));
    CHKERRQ(VecGetArray(v,&pv));
    CHKERRMPI(MPI_Bcast(pv,n,MPIU_SCALAR,0,PetscObjectComm((PetscObject)fn)));
    CHKERRQ(VecRestoreArray(v,&pv));
  }
  PetscFunctionReturn(0);
}

/*@
   FNEvaluateFunctionMatVec - Computes the first column of the matrix f(A)
   for a given matrix A.

   Logically Collective on fn

   Input Parameters:
+  fn - the math function context
-  A  - matrix on which the function must be evaluated

   Output Parameter:
.  v  - vector to hold the first column of f(A)

   Notes:
   This operation is similar to FNEvaluateFunctionMat() but returns only
   the first column of f(A), hence saving computations in most cases.

   Level: advanced

.seealso: FNEvaluateFunction(), FNEvaluateFunctionMat(), FNSetMethod()
@*/
PetscErrorCode FNEvaluateFunctionMatVec(FN fn,Mat A,Vec v)
{
  PetscInt       m,n;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidHeaderSpecific(A,MAT_CLASSID,2);
  PetscValidHeaderSpecific(v,VEC_CLASSID,3);
  PetscValidType(fn,1);
  PetscValidType(A,2);
  PetscValidType(v,3);
  PetscCheckTypeName(A,MATSEQDENSE);
  CHKERRQ(MatGetSize(A,&m,&n));
  PetscCheck(m==n,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_SIZ,"Mat A is not square (has %" PetscInt_FMT " rows, %" PetscInt_FMT " cols)",m,n);
  CHKERRQ(VecGetSize(v,&m));
  PetscCheck(m==n,PetscObjectComm((PetscObject)fn),PETSC_ERR_ARG_SIZ,"Matrix A and vector v must have the same size");
  CHKERRQ(PetscLogEventBegin(FN_Evaluate,fn,0,0,0));
  CHKERRQ(FNEvaluateFunctionMatVec_Private(fn,A,v,PETSC_TRUE));
  CHKERRQ(PetscLogEventEnd(FN_Evaluate,fn,0,0,0));
  PetscFunctionReturn(0);
}

/*@
   FNSetFromOptions - Sets FN options from the options database.

   Collective on fn

   Input Parameters:
.  fn - the math function context

   Notes:
   To see all options, run your program with the -help option.

   Level: beginner

.seealso: FNSetOptionsPrefix()
@*/
PetscErrorCode FNSetFromOptions(FN fn)
{
  PetscErrorCode ierr;
  char           type[256];
  PetscScalar    array[2];
  PetscInt       k,meth;
  PetscBool      flg;
  FNParallelType pmode;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  CHKERRQ(FNRegisterAll());
  ierr = PetscObjectOptionsBegin((PetscObject)fn);CHKERRQ(ierr);
    CHKERRQ(PetscOptionsFList("-fn_type","Math function type","FNSetType",FNList,(char*)(((PetscObject)fn)->type_name?((PetscObject)fn)->type_name:FNRATIONAL),type,sizeof(type),&flg));
    if (flg) CHKERRQ(FNSetType(fn,type));
    else if (!((PetscObject)fn)->type_name) CHKERRQ(FNSetType(fn,FNRATIONAL));

    k = 2;
    array[0] = 0.0; array[1] = 0.0;
    CHKERRQ(PetscOptionsScalarArray("-fn_scale","Scale factors (one or two scalar values separated with a comma without spaces)","FNSetScale",array,&k,&flg));
    if (flg) {
      if (k<2) array[1] = 1.0;
      CHKERRQ(FNSetScale(fn,array[0],array[1]));
    }

    CHKERRQ(PetscOptionsInt("-fn_method","Method to be used for computing matrix functions","FNSetMethod",fn->method,&meth,&flg));
    if (flg) CHKERRQ(FNSetMethod(fn,meth));

    CHKERRQ(PetscOptionsEnum("-fn_parallel","Operation mode in parallel runs","FNSetParallel",FNParallelTypes,(PetscEnum)fn->pmode,(PetscEnum*)&pmode,&flg));
    if (flg) CHKERRQ(FNSetParallel(fn,pmode));

    if (fn->ops->setfromoptions) CHKERRQ((*fn->ops->setfromoptions)(PetscOptionsObject,fn));
    CHKERRQ(PetscObjectProcessOptionsHandlers(PetscOptionsObject,(PetscObject)fn));
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
   FNView - Prints the FN data structure.

   Collective on fn

   Input Parameters:
+  fn - the math function context
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

.seealso: FNCreate()
@*/
PetscErrorCode FNView(FN fn,PetscViewer viewer)
{
  PetscBool      isascii;
  PetscMPIInt    size;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  if (!viewer) CHKERRQ(PetscViewerASCIIGetStdout(PetscObjectComm((PetscObject)fn),&viewer));
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_CLASSID,2);
  PetscCheckSameComm(fn,1,viewer,2);
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii));
  if (isascii) {
    CHKERRQ(PetscObjectPrintClassNamePrefixType((PetscObject)fn,viewer));
    CHKERRMPI(MPI_Comm_size(PetscObjectComm((PetscObject)fn),&size));
    if (size>1) CHKERRQ(PetscViewerASCIIPrintf(viewer,"  parallel operation mode: %s\n",FNParallelTypes[fn->pmode]));
    if (fn->ops->view) {
      CHKERRQ(PetscViewerASCIIPushTab(viewer));
      CHKERRQ((*fn->ops->view)(fn,viewer));
      CHKERRQ(PetscViewerASCIIPopTab(viewer));
    }
  }
  PetscFunctionReturn(0);
}

/*@C
   FNViewFromOptions - View from options

   Collective on FN

   Input Parameters:
+  fn   - the math function context
.  obj  - optional object
-  name - command line option

   Level: intermediate

.seealso: FNView(), FNCreate()
@*/
PetscErrorCode FNViewFromOptions(FN fn,PetscObject obj,const char name[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  CHKERRQ(PetscObjectViewFromOptions((PetscObject)fn,obj,name));
  PetscFunctionReturn(0);
}

/*@
   FNDuplicate - Duplicates a math function, copying all parameters, possibly with a
   different communicator.

   Collective on fn

   Input Parameters:
+  fn   - the math function context
-  comm - MPI communicator

   Output Parameter:
.  newfn - location to put the new FN context

   Note:
   In order to use the same MPI communicator as in the original object,
   use PetscObjectComm((PetscObject)fn).

   Level: developer

.seealso: FNCreate()
@*/
PetscErrorCode FNDuplicate(FN fn,MPI_Comm comm,FN *newfn)
{
  FNType         type;
  PetscScalar    alpha,beta;
  PetscInt       meth;
  FNParallelType ptype;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(fn,FN_CLASSID,1);
  PetscValidType(fn,1);
  PetscValidPointer(newfn,3);
  CHKERRQ(FNCreate(comm,newfn));
  CHKERRQ(FNGetType(fn,&type));
  CHKERRQ(FNSetType(*newfn,type));
  CHKERRQ(FNGetScale(fn,&alpha,&beta));
  CHKERRQ(FNSetScale(*newfn,alpha,beta));
  CHKERRQ(FNGetMethod(fn,&meth));
  CHKERRQ(FNSetMethod(*newfn,meth));
  CHKERRQ(FNGetParallel(fn,&ptype));
  CHKERRQ(FNSetParallel(*newfn,ptype));
  if (fn->ops->duplicate) CHKERRQ((*fn->ops->duplicate)(fn,comm,newfn));
  PetscFunctionReturn(0);
}

/*@C
   FNDestroy - Destroys FN context that was created with FNCreate().

   Collective on fn

   Input Parameter:
.  fn - the math function context

   Level: beginner

.seealso: FNCreate()
@*/
PetscErrorCode FNDestroy(FN *fn)
{
  PetscInt       i;

  PetscFunctionBegin;
  if (!*fn) PetscFunctionReturn(0);
  PetscValidHeaderSpecific(*fn,FN_CLASSID,1);
  if (--((PetscObject)(*fn))->refct > 0) { *fn = 0; PetscFunctionReturn(0); }
  if ((*fn)->ops->destroy) CHKERRQ((*(*fn)->ops->destroy)(*fn));
  for (i=0;i<(*fn)->nw;i++) CHKERRQ(MatDestroy(&(*fn)->W[i]));
  CHKERRQ(PetscHeaderDestroy(fn));
  PetscFunctionReturn(0);
}

/*@C
   FNRegister - Adds a mathematical function to the FN package.

   Not collective

   Input Parameters:
+  name - name of a new user-defined FN
-  function - routine to create context

   Notes:
   FNRegister() may be called multiple times to add several user-defined functions.

   Level: advanced

.seealso: FNRegisterAll()
@*/
PetscErrorCode FNRegister(const char *name,PetscErrorCode (*function)(FN))
{
  PetscFunctionBegin;
  CHKERRQ(FNInitializePackage());
  CHKERRQ(PetscFunctionListAdd(&FNList,name,function));
  PetscFunctionReturn(0);
}
