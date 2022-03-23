/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Polygonal region defined by a set of vertices
*/

#include <slepc/private/rgimpl.h>      /*I "slepcrg.h" I*/
#include <petscdraw.h>

#define VERTMAX 30

typedef struct {
  PetscInt    n;         /* number of vertices */
  PetscScalar *vr,*vi;   /* array of vertices (vi not used in complex scalars) */
} RG_POLYGON;

PetscErrorCode RGComputeBoundingBox_Polygon(RG,PetscReal*,PetscReal*,PetscReal*,PetscReal*);

#if !defined(PETSC_USE_COMPLEX)
static PetscBool CheckSymmetry(PetscInt n,PetscScalar *vr,PetscScalar *vi)
{
  PetscInt i,j,k;
  /* find change of sign in imaginary part */
  j = vi[0]!=0.0? 0: 1;
  for (k=j+1;k<n;k++) {
    if (vi[k]!=0.0) {
      if (vi[k]*vi[j]<0.0) break;
      j++;
    }
  }
  if (k==n) return (j==1)? PETSC_TRUE: PETSC_FALSE;
  /* check pairing vertices */
  for (i=0;i<n/2;i++) {
    if (vr[k]!=vr[j] || vi[k]!=-vi[j]) return PETSC_FALSE;
    k = (k+1)%n;
    j = (j-1+n)%n;
  }
  return PETSC_TRUE;
}
#endif

static PetscErrorCode RGPolygonSetVertices_Polygon(RG rg,PetscInt n,PetscScalar *vr,PetscScalar *vi)
{
  PetscInt       i;
  RG_POLYGON     *ctx = (RG_POLYGON*)rg->data;

  PetscFunctionBegin;
  PetscCheck(n>2,PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_OUTOFRANGE,"At least 3 vertices required, you provided %" PetscInt_FMT,n);
  PetscCheck(n<=VERTMAX,PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_OUTOFRANGE,"Too many points, maximum allowed is %d",VERTMAX);
#if !defined(PETSC_USE_COMPLEX)
  PetscCheck(CheckSymmetry(n,vr,vi),PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"In real scalars the region must be symmetric wrt real axis");
#endif
  if (ctx->n) {
    CHKERRQ(PetscFree(ctx->vr));
#if !defined(PETSC_USE_COMPLEX)
    CHKERRQ(PetscFree(ctx->vi));
#endif
  }
  ctx->n = n;
  CHKERRQ(PetscMalloc1(n,&ctx->vr));
#if !defined(PETSC_USE_COMPLEX)
  CHKERRQ(PetscMalloc1(n,&ctx->vi));
#endif
  for (i=0;i<n;i++) {
    ctx->vr[i] = vr[i];
#if !defined(PETSC_USE_COMPLEX)
    ctx->vi[i] = vi[i];
#endif
  }
  PetscFunctionReturn(0);
}

/*@
   RGPolygonSetVertices - Sets the vertices that define the polygon region.

   Logically Collective on rg

   Input Parameters:
+  rg - the region context
.  n  - number of vertices
.  vr - array of vertices
-  vi - array of vertices (imaginary part)

   Options Database Keys:
+  -rg_polygon_vertices - Sets the vertices
-  -rg_polygon_verticesi - Sets the vertices (imaginary part)

   Notes:
   In the case of complex scalars, only argument vr is used, containing
   the complex vertices; the list of vertices can be provided in the
   command line with a comma-separated list of complex values
   [+/-][realnumber][+/-]realnumberi with no spaces.

   When PETSc is built with real scalars, the real and imaginary parts of
   the vertices must be provided in two separate arrays (or two lists in
   the command line). In this case, the region must be symmetric with
   respect to the real axis.

   Level: advanced

.seealso: RGPolygonGetVertices()
@*/
PetscErrorCode RGPolygonSetVertices(RG rg,PetscInt n,PetscScalar vr[],PetscScalar vi[])
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(rg,RG_CLASSID,1);
  PetscValidLogicalCollectiveInt(rg,n,2);
  PetscValidScalarPointer(vr,3);
#if !defined(PETSC_USE_COMPLEX)
  PetscValidScalarPointer(vi,4);
#endif
  CHKERRQ(PetscTryMethod(rg,"RGPolygonSetVertices_C",(RG,PetscInt,PetscScalar*,PetscScalar*),(rg,n,vr,vi)));
  PetscFunctionReturn(0);
}

static PetscErrorCode RGPolygonGetVertices_Polygon(RG rg,PetscInt *n,PetscScalar **vr,PetscScalar **vi)
{
  RG_POLYGON     *ctx = (RG_POLYGON*)rg->data;
  PetscInt       i;

  PetscFunctionBegin;
  if (n) *n  = ctx->n;
  if (vr) {
    if (!ctx->n) *vr = NULL;
    else {
      CHKERRQ(PetscMalloc1(ctx->n,vr));
      for (i=0;i<ctx->n;i++) (*vr)[i] = ctx->vr[i];
    }
  }
#if !defined(PETSC_USE_COMPLEX)
  if (vi) {
    if (!ctx->n) *vi = NULL;
    else {
      CHKERRQ(PetscMalloc1(ctx->n,vi));
      for (i=0;i<ctx->n;i++) (*vi)[i] = ctx->vi[i];
    }
  }
#endif
  PetscFunctionReturn(0);
}

/*@C
   RGPolygonGetVertices - Gets the vertices that define the polygon region.

   Not Collective

   Input Parameter:
.  rg     - the region context

   Output Parameters:
+  n  - number of vertices
.  vr - array of vertices
-  vi - array of vertices (imaginary part)

   Notes:
   The values passed by user with RGPolygonSetVertices() are returned (or null
   pointers otherwise).
   The returned arrays should be freed by the user when no longer needed.

   Level: advanced

.seealso: RGPolygonSetVertices()
@*/
PetscErrorCode RGPolygonGetVertices(RG rg,PetscInt *n,PetscScalar **vr,PetscScalar **vi)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(rg,RG_CLASSID,1);
  CHKERRQ(PetscUseMethod(rg,"RGPolygonGetVertices_C",(RG,PetscInt*,PetscScalar**,PetscScalar**),(rg,n,vr,vi)));
  PetscFunctionReturn(0);
}

PetscErrorCode RGView_Polygon(RG rg,PetscViewer viewer)
{
  RG_POLYGON     *ctx = (RG_POLYGON*)rg->data;
  PetscBool      isdraw,isascii;
  int            winw,winh;
  PetscDraw      draw;
  PetscDrawAxis  axis;
  PetscReal      a,b,c,d,ab,cd,lx,ly,w,x0,y0,x1,y1,scale=1.2;
  PetscInt       i;
  char           str[50];

  PetscFunctionBegin;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERDRAW,&isdraw));
  CHKERRQ(PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii));
  if (isascii) {
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"  vertices: "));
    CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_FALSE));
    for (i=0;i<ctx->n;i++) {
#if defined(PETSC_USE_COMPLEX)
      CHKERRQ(SlepcSNPrintfScalar(str,sizeof(str),ctx->vr[i],PETSC_FALSE));
#else
      if (ctx->vi[i]!=0.0) CHKERRQ(PetscSNPrintf(str,sizeof(str),"%g%+gi",(double)ctx->vr[i],(double)ctx->vi[i]));
      else CHKERRQ(PetscSNPrintf(str,sizeof(str),"%g",(double)ctx->vr[i]));
#endif
      CHKERRQ(PetscViewerASCIIPrintf(viewer,"%s%s",str,(i<ctx->n-1)?", ":""));
    }
    CHKERRQ(PetscViewerASCIIPrintf(viewer,"\n"));
    CHKERRQ(PetscViewerASCIIUseTabs(viewer,PETSC_TRUE));
  } else if (isdraw) {
    CHKERRQ(PetscViewerDrawGetDraw(viewer,0,&draw));
    CHKERRQ(PetscDrawCheckResizedWindow(draw));
    CHKERRQ(PetscDrawGetWindowSize(draw,&winw,&winh));
    winw = PetscMax(winw,1); winh = PetscMax(winh,1);
    CHKERRQ(PetscDrawClear(draw));
    CHKERRQ(PetscDrawSetTitle(draw,"Polygonal region"));
    CHKERRQ(PetscDrawAxisCreate(draw,&axis));
    CHKERRQ(RGComputeBoundingBox_Polygon(rg,&a,&b,&c,&d));
    a *= rg->sfactor;
    b *= rg->sfactor;
    c *= rg->sfactor;
    d *= rg->sfactor;
    lx = b-a;
    ly = d-c;
    ab = (a+b)/2;
    cd = (c+d)/2;
    w  = scale*PetscMax(lx/winw,ly/winh)/2;
    CHKERRQ(PetscDrawAxisSetLimits(axis,ab-w*winw,ab+w*winw,cd-w*winh,cd+w*winh));
    CHKERRQ(PetscDrawAxisDraw(axis));
    CHKERRQ(PetscDrawAxisDestroy(&axis));
    for (i=0;i<ctx->n;i++) {
#if defined(PETSC_USE_COMPLEX)
      x0 = PetscRealPart(ctx->vr[i]); y0 = PetscImaginaryPart(ctx->vr[i]);
      if (i<ctx->n-1) {
        x1 = PetscRealPart(ctx->vr[i+1]); y1 = PetscImaginaryPart(ctx->vr[i+1]);
      } else {
        x1 = PetscRealPart(ctx->vr[0]); y1 = PetscImaginaryPart(ctx->vr[0]);
      }
#else
      x0 = ctx->vr[i]; y0 = ctx->vi[i];
      if (i<ctx->n-1) {
        x1 = ctx->vr[i+1]; y1 = ctx->vi[i+1];
      } else {
        x1 = ctx->vr[0]; y1 = ctx->vi[0];
      }
#endif
      CHKERRQ(PetscDrawLine(draw,x0*rg->sfactor,y0*rg->sfactor,x1*rg->sfactor,y1*rg->sfactor,PETSC_DRAW_MAGENTA));
    }
    CHKERRQ(PetscDrawFlush(draw));
    CHKERRQ(PetscDrawSave(draw));
    CHKERRQ(PetscDrawPause(draw));
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGIsTrivial_Polygon(RG rg,PetscBool *trivial)
{
  RG_POLYGON *ctx = (RG_POLYGON*)rg->data;

  PetscFunctionBegin;
  *trivial = PetscNot(ctx->n);
  PetscFunctionReturn(0);
}

PetscErrorCode RGComputeContour_Polygon(RG rg,PetscInt n,PetscScalar *ucr,PetscScalar *uci)
{
  RG_POLYGON     *ctx = (RG_POLYGON*)rg->data;
  PetscReal      length,h,d,rem=0.0;
  PetscInt       k=1,idx=ctx->n-1,i;
  PetscBool      ini=PETSC_FALSE;
  PetscScalar    incr,*cr=ucr,*ci=uci;
#if !defined(PETSC_USE_COMPLEX)
  PetscScalar    inci;
#endif

  PetscFunctionBegin;
  PetscCheck(ctx->n,PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONGSTATE,"No vertices have been set yet");
  length = SlepcAbsEigenvalue(ctx->vr[0]-ctx->vr[ctx->n-1],ctx->vi[0]-ctx->vi[ctx->n-1]);
  for (i=0;i<ctx->n-1;i++) length += SlepcAbsEigenvalue(ctx->vr[i]-ctx->vr[i+1],ctx->vi[i]-ctx->vi[i+1]);
  h = length/n;
  if (!ucr) CHKERRQ(PetscMalloc1(n,&cr));
  if (!uci) CHKERRQ(PetscMalloc1(n,&ci));
  cr[0] = ctx->vr[0];
#if !defined(PETSC_USE_COMPLEX)
  ci[0] = ctx->vi[0];
#endif
  incr = ctx->vr[ctx->n-1]-ctx->vr[0];
#if !defined(PETSC_USE_COMPLEX)
  inci = ctx->vi[ctx->n-1]-ctx->vi[0];
#endif
  d = SlepcAbsEigenvalue(incr,inci);
  incr /= d;
#if !defined(PETSC_USE_COMPLEX)
  inci /= d;
#endif
  while (k<n) {
    if (ini) {
      incr = ctx->vr[idx]-ctx->vr[idx+1];
#if !defined(PETSC_USE_COMPLEX)
      inci = ctx->vi[idx]-ctx->vi[idx+1];
#endif
      d = SlepcAbsEigenvalue(incr,inci);
      incr /= d;
#if !defined(PETSC_USE_COMPLEX)
      inci /= d;
#endif
      if (rem+d>h) {
        cr[k] = ctx->vr[idx+1]+incr*(h-rem);
#if !defined(PETSC_USE_COMPLEX)
        ci[k] = ctx->vi[idx+1]+inci*(h-rem);
#endif
        k++;
        ini = PETSC_FALSE;
      } else {rem += d; idx--;}
    } else {
#if !defined(PETSC_USE_COMPLEX)
      rem = SlepcAbsEigenvalue(ctx->vr[idx]-cr[k-1],ctx->vi[idx]-ci[k-1]);
#else
      rem = PetscAbsScalar(ctx->vr[idx]-cr[k-1]);
#endif
      if (rem>h) {
        cr[k] = cr[k-1]+incr*h;
#if !defined(PETSC_USE_COMPLEX)
        ci[k] = ci[k-1]+inci*h;
#endif
        k++;
      } else {ini = PETSC_TRUE; idx--;}
    }
  }
  if (!ucr) CHKERRQ(PetscFree(cr));
  if (!uci) CHKERRQ(PetscFree(ci));
  PetscFunctionReturn(0);
}

PetscErrorCode RGComputeBoundingBox_Polygon(RG rg,PetscReal *a,PetscReal *b,PetscReal *c,PetscReal *d)
{
  RG_POLYGON *ctx = (RG_POLYGON*)rg->data;
  PetscInt   i;

  PetscFunctionBegin;
  if (a) *a =  PETSC_MAX_REAL;
  if (b) *b = -PETSC_MAX_REAL;
  if (c) *c =  PETSC_MAX_REAL;
  if (d) *d = -PETSC_MAX_REAL;
  for (i=0;i<ctx->n;i++) {
#if defined(PETSC_USE_COMPLEX)
    if (a) *a = PetscMin(*a,PetscRealPart(ctx->vr[i]));
    if (b) *b = PetscMax(*b,PetscRealPart(ctx->vr[i]));
    if (c) *c = PetscMin(*c,PetscImaginaryPart(ctx->vr[i]));
    if (d) *d = PetscMax(*d,PetscImaginaryPart(ctx->vr[i]));
#else
    if (a) *a = PetscMin(*a,ctx->vr[i]);
    if (b) *b = PetscMax(*b,ctx->vr[i]);
    if (c) *c = PetscMin(*c,ctx->vi[i]);
    if (d) *d = PetscMax(*d,ctx->vi[i]);
#endif
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGCheckInside_Polygon(RG rg,PetscReal px,PetscReal py,PetscInt *inout)
{
  RG_POLYGON *ctx = (RG_POLYGON*)rg->data;
  PetscReal  val,x[VERTMAX],y[VERTMAX];
  PetscBool  mx,my,nx,ny;
  PetscInt   i,j;

  PetscFunctionBegin;
  for (i=0;i<ctx->n;i++) {
#if defined(PETSC_USE_COMPLEX)
    x[i] = PetscRealPart(ctx->vr[i])-px;
    y[i] = PetscImaginaryPart(ctx->vr[i])-py;
#else
    x[i] = ctx->vr[i]-px;
    y[i] = ctx->vi[i]-py;
#endif
  }
  *inout = -1;
  for (i=0;i<ctx->n;i++) {
    j = (i+1)%ctx->n;
    mx = PetscNot(x[i]<0.0);
    nx = PetscNot(x[j]<0.0);
    my = PetscNot(y[i]<0.0);
    ny = PetscNot(y[j]<0.0);
    if (!((my||ny) && (mx||nx)) || (mx&&nx)) continue;
    if (((my && ny && (mx||nx)) && (!(mx&&nx)))) {
      *inout = -*inout;
      continue;
    }
    val = (y[i]*x[j]-x[i]*y[j])/(x[j]-x[i]);
    if (PetscAbs(val)<10*PETSC_MACHINE_EPSILON) {
      *inout = 0;
      PetscFunctionReturn(0);
    } else if (val>0.0) *inout = -*inout;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGSetFromOptions_Polygon(PetscOptionItems *PetscOptionsObject,RG rg)
{
  PetscScalar    array[VERTMAX];
  PetscInt       i,k;
  PetscBool      flg,flgi=PETSC_FALSE;
#if !defined(PETSC_USE_COMPLEX)
  PetscScalar    arrayi[VERTMAX];
  PetscInt       ki;
#else
  PetscScalar    *arrayi=NULL;
#endif

  PetscFunctionBegin;
  CHKERRQ(PetscOptionsHead(PetscOptionsObject,"RG Polygon Options"));

    k = VERTMAX;
    for (i=0;i<k;i++) array[i] = 0;
    CHKERRQ(PetscOptionsScalarArray("-rg_polygon_vertices","Vertices of polygon","RGPolygonSetVertices",array,&k,&flg));
#if !defined(PETSC_USE_COMPLEX)
    ki = VERTMAX;
    for (i=0;i<ki;i++) arrayi[i] = 0;
    CHKERRQ(PetscOptionsScalarArray("-rg_polygon_verticesi","Vertices of polygon (imaginary part)","RGPolygonSetVertices",arrayi,&ki,&flgi));
    PetscCheck(ki==k,PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_SIZ,"The number of real %" PetscInt_FMT " and imaginary %" PetscInt_FMT " parts do not match",k,ki);
#endif
    if (flg || flgi) CHKERRQ(RGPolygonSetVertices(rg,k,array,arrayi));

  CHKERRQ(PetscOptionsTail());
  PetscFunctionReturn(0);
}

PetscErrorCode RGDestroy_Polygon(RG rg)
{
  RG_POLYGON     *ctx = (RG_POLYGON*)rg->data;

  PetscFunctionBegin;
  if (ctx->n) {
    CHKERRQ(PetscFree(ctx->vr));
#if !defined(PETSC_USE_COMPLEX)
    CHKERRQ(PetscFree(ctx->vi));
#endif
  }
  CHKERRQ(PetscFree(rg->data));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)rg,"RGPolygonSetVertices_C",NULL));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)rg,"RGPolygonGetVertices_C",NULL));
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode RGCreate_Polygon(RG rg)
{
  RG_POLYGON     *polygon;

  PetscFunctionBegin;
  CHKERRQ(PetscNewLog(rg,&polygon));
  rg->data = (void*)polygon;

  rg->ops->istrivial      = RGIsTrivial_Polygon;
  rg->ops->computecontour = RGComputeContour_Polygon;
  rg->ops->computebbox    = RGComputeBoundingBox_Polygon;
  rg->ops->checkinside    = RGCheckInside_Polygon;
  rg->ops->setfromoptions = RGSetFromOptions_Polygon;
  rg->ops->view           = RGView_Polygon;
  rg->ops->destroy        = RGDestroy_Polygon;
  CHKERRQ(PetscObjectComposeFunction((PetscObject)rg,"RGPolygonSetVertices_C",RGPolygonSetVertices_Polygon));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)rg,"RGPolygonGetVertices_C",RGPolygonGetVertices_Polygon));
  PetscFunctionReturn(0);
}
