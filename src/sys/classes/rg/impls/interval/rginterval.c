/*
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   SLEPc - Scalable Library for Eigenvalue Problem Computations
   Copyright (c) 2002-2021, Universitat Politecnica de Valencia, Spain

   This file is part of SLEPc.
   SLEPc is distributed under a 2-clause BSD license (see LICENSE).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
*/
/*
   Interval of the real axis or more generally a (possibly open) rectangle
   of the complex plane
*/

#include <slepc/private/rgimpl.h>      /*I "slepcrg.h" I*/
#include <petscdraw.h>

typedef struct {
  PetscReal   a,b;     /* interval in the real axis */
  PetscReal   c,d;     /* interval in the imaginary axis */
} RG_INTERVAL;

static PetscErrorCode RGIntervalSetEndpoints_Interval(RG rg,PetscReal a,PetscReal b,PetscReal c,PetscReal d)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (!a && !b && !c && !d) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"At least one argument must be nonzero");
  if (a==b && a) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"Badly defined interval, endpoints must be distinct (or both zero)");
  if (a>b) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"Badly defined interval, must be a<b");
  if (c==d && c) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"Badly defined interval, endpoints must be distinct (or both zero)");
  if (c>d) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"Badly defined interval, must be c<d");
#if !defined(PETSC_USE_COMPLEX)
  if (c!=-d) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_WRONG,"In real scalars the region must be symmetric wrt real axis");
#endif
  ctx->a = a;
  ctx->b = b;
  ctx->c = c;
  ctx->d = d;
  PetscFunctionReturn(0);
}

/*@
   RGIntervalSetEndpoints - Sets the parameters defining the interval region.

   Logically Collective on rg

   Input Parameters:
+  rg  - the region context
.  a - left endpoint of the interval in the real axis
.  b - right endpoint of the interval in the real axis
.  c - bottom endpoint of the interval in the imaginary axis
-  d - top endpoint of the interval in the imaginary axis

   Options Database Keys:
.  -rg_interval_endpoints - the four endpoints

   Note:
   The region is defined as [a,b]x[c,d]. Particular cases are an interval on
   the real axis (c=d=0), similar for the imaginary axis (a=b=0), the whole
   complex plane (a=-inf,b=inf,c=-inf,d=inf), and so on.

   When PETSc is built with real scalars, the region must be symmetric with
   respect to the real axis.

   Level: advanced

.seealso: RGIntervalGetEndpoints()
@*/
PetscErrorCode RGIntervalSetEndpoints(RG rg,PetscReal a,PetscReal b,PetscReal c,PetscReal d)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(rg,RG_CLASSID,1);
  PetscValidLogicalCollectiveReal(rg,a,2);
  PetscValidLogicalCollectiveReal(rg,b,3);
  PetscValidLogicalCollectiveReal(rg,c,4);
  PetscValidLogicalCollectiveReal(rg,d,5);
  ierr = PetscTryMethod(rg,"RGIntervalSetEndpoints_C",(RG,PetscReal,PetscReal,PetscReal,PetscReal),(rg,a,b,c,d));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode RGIntervalGetEndpoints_Interval(RG rg,PetscReal *a,PetscReal *b,PetscReal *c,PetscReal *d)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (a) *a = ctx->a;
  if (b) *b = ctx->b;
  if (c) *c = ctx->c;
  if (d) *d = ctx->d;
  PetscFunctionReturn(0);
}

/*@C
   RGIntervalGetEndpoints - Gets the parameters that define the interval region.

   Not Collective

   Input Parameter:
.  rg - the region context

   Output Parameters:
+  a - left endpoint of the interval in the real axis
.  b - right endpoint of the interval in the real axis
.  c - bottom endpoint of the interval in the imaginary axis
-  d - top endpoint of the interval in the imaginary axis

   Level: advanced

.seealso: RGIntervalSetEndpoints()
@*/
PetscErrorCode RGIntervalGetEndpoints(RG rg,PetscReal *a,PetscReal *b,PetscReal *c,PetscReal *d)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(rg,RG_CLASSID,1);
  ierr = PetscUseMethod(rg,"RGIntervalGetEndpoints_C",(RG,PetscReal*,PetscReal*,PetscReal*,PetscReal*),(rg,a,b,c,d));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode RGView_Interval(RG rg,PetscViewer viewer)
{
  PetscErrorCode ierr;
  RG_INTERVAL    *ctx = (RG_INTERVAL*)rg->data;
  PetscBool      isdraw,isascii;
  int            winw,winh;
  PetscDraw      draw;
  PetscDrawAxis  axis;
  PetscReal      a,b,c,d,ab,cd,lx,ly,w,scale=1.2;

  PetscFunctionBegin;
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERDRAW,&isdraw);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)viewer,PETSCVIEWERASCII,&isascii);CHKERRQ(ierr);
  if (isascii) {
    ierr = PetscViewerASCIIPrintf(viewer,"  region: [%g,%g]x[%g,%g]\n",RGShowReal(ctx->a),RGShowReal(ctx->b),RGShowReal(ctx->c),RGShowReal(ctx->d));CHKERRQ(ierr);
  } else if (isdraw) {
    ierr = PetscViewerDrawGetDraw(viewer,0,&draw);CHKERRQ(ierr);
    ierr = PetscDrawCheckResizedWindow(draw);CHKERRQ(ierr);
    ierr = PetscDrawGetWindowSize(draw,&winw,&winh);CHKERRQ(ierr);
    winw = PetscMax(winw,1); winh = PetscMax(winh,1);
    ierr = PetscDrawClear(draw);CHKERRQ(ierr);
    ierr = PetscDrawSetTitle(draw,"Interval region");CHKERRQ(ierr);
    ierr = PetscDrawAxisCreate(draw,&axis);CHKERRQ(ierr);
    a  = ctx->a*rg->sfactor;
    b  = ctx->b*rg->sfactor;
    c  = ctx->c*rg->sfactor;
    d  = ctx->d*rg->sfactor;
    lx = b-a;
    ly = d-c;
    ab = (a+b)/2;
    cd = (c+d)/2;
    w  = scale*PetscMax(lx/winw,ly/winh)/2;
    ierr = PetscDrawAxisSetLimits(axis,ab-w*winw,ab+w*winw,cd-w*winh,cd+w*winh);CHKERRQ(ierr);
    ierr = PetscDrawAxisDraw(axis);CHKERRQ(ierr);
    ierr = PetscDrawAxisDestroy(&axis);CHKERRQ(ierr);
    ierr = PetscDrawRectangle(draw,a,c,b,d,PETSC_DRAW_BLUE,PETSC_DRAW_BLUE,PETSC_DRAW_BLUE,PETSC_DRAW_BLUE);CHKERRQ(ierr);
    ierr = PetscDrawFlush(draw);CHKERRQ(ierr);
    ierr = PetscDrawSave(draw);CHKERRQ(ierr);
    ierr = PetscDrawPause(draw);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGIsTrivial_Interval(RG rg,PetscBool *trivial)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (rg->complement) *trivial = (ctx->a==ctx->b && ctx->c==ctx->d)? PETSC_TRUE: PETSC_FALSE;
  else *trivial = (ctx->a<=-PETSC_MAX_REAL && ctx->b>=PETSC_MAX_REAL && ctx->c<=-PETSC_MAX_REAL && ctx->d>=PETSC_MAX_REAL)? PETSC_TRUE: PETSC_FALSE;
  PetscFunctionReturn(0);
}

PetscErrorCode RGComputeContour_Interval(RG rg,PetscInt n,PetscScalar *cr,PetscScalar *ci)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;
  PetscInt    i,N;
  PetscReal   theta,pr,pi,y,crt,cit,s,d;
  PetscScalar center;

  PetscFunctionBegin;
  if (!(ctx->a>-PETSC_MAX_REAL && ctx->b<PETSC_MAX_REAL && ctx->c>-PETSC_MAX_REAL && ctx->d<PETSC_MAX_REAL)) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_SUP,"Contour not defined in unbounded regions");
#if defined(PETSC_USE_COMPLEX)
  center = PetscCMPLX((ctx->a+ctx->b)/2.0,(ctx->c+ctx->d)/2.0);
#else
  center = (ctx->a+ctx->b)/2.0;
#endif
  if (ctx->a==ctx->b || ctx->c==ctx->d) {
    d = (ctx->a==ctx->b)?ctx->d-ctx->c:ctx->b-ctx->a;
    for (i=0;i<n;i++) {
      theta = (i+.5)*PETSC_PI/n;
      y = .5*d*PetscCosReal(theta);
      if (ctx->a==ctx->b) {cit = y; crt = 0.0;}
      else {cit = 0.0; crt = y;}
#if defined(PETSC_USE_COMPLEX)
      cr[i] = center+PetscCMPLX(crt,cit);
#else
      if (cr) cr[i] = center+crt;
      if (ci) ci[i] = cit;
#endif
    }
  } else {
    N = n/2;
    for (i=0;i<N;i++) {
      theta = (i+.5)*PETSC_PI/N;
      pr = .5*(ctx->b-ctx->a)*PetscCosReal(theta);
      pi = PetscSinReal(theta)*(ctx->d-ctx->c)*.5;
      y =(pr!=0.0)?(ctx->b-ctx->a)*.5*pi/pr:PETSC_MAX_REAL;
      if (PetscAbsReal(y)<(ctx->d-ctx->c)*.5) {
        s = (y>=0.0)?1.0:-1.0;
        crt = s*(ctx->b-ctx->a)*.5;
        cit = s*y;
      } else {
        crt = (ctx->d-ctx->c)*.5*pr/pi;
        cit = .5*(ctx->d-ctx->c);
      }
#if defined(PETSC_USE_COMPLEX)
      cr[i] = PetscCMPLX(crt,cit);
      cr[N+i] = -cr[i];
      if (ci) { ci[i] = 0.0; ci[N+i] = 0.0;}
#else
      cr[i] = crt; ci[i] = cit;
      cr[N+i] = -cr[i];
      ci[N+i] = -ci[i];
#endif
    }
    for (i=0;i<2*N;i++) cr[i] += center;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGComputeBoundingBox_Interval(RG rg,PetscReal *a,PetscReal *b,PetscReal *c,PetscReal *d)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (a) *a = ctx->a;
  if (b) *b = ctx->b;
  if (c) *c = ctx->c;
  if (d) *d = ctx->d;
  PetscFunctionReturn(0);
}

PetscErrorCode RGComputeQuadrature_Interval(RG rg,RGQuadRule quad,PetscInt n,PetscScalar *z,PetscScalar *zn,PetscScalar *w)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;
  PetscReal   theta,max_w=0.0,radius=1.0;
  PetscScalar tmp,tmp2,center=0.0;
  PetscInt    i,j;

  PetscFunctionBegin;
  if (quad == RG_QUADRULE_CHEBYSHEV) {
    if ((ctx->c!=ctx->d || ctx->c!=0.0) && (ctx->a!=ctx->b || ctx->a!=0.0)) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_SUP,"Endpoints of the imaginary axis or the real axis must be both zero");
    for (i=0;i<n;i++) {
      theta = PETSC_PI*(i+0.5)/n;
      zn[i] = PetscCosReal(theta);
      w[i]  = PetscCosReal((n-1)*theta)/n;
      if (ctx->c==ctx->d) z[i] = ((ctx->b-ctx->a)*(zn[i]+1.0)/2.0+ctx->a)*rg->sfactor;
      else if (ctx->a==ctx->b) {
#if defined(PETSC_USE_COMPLEX)
        z[i] = ((ctx->d-ctx->c)*(zn[i]+1.0)/2.0+ctx->c)*rg->sfactor*PETSC_i;
#else
        SETERRQ(PETSC_COMM_SELF,PETSC_ERR_SUP,"Integration points on a vertical line require complex arithmetic");
#endif
      }
    }
  } else {  /* RG_QUADRULE_TRAPEZOIDAL */
    center = rg->sfactor*((ctx->b+ctx->a)/2.0+(ctx->d+ctx->c)/2.0*PETSC_PI);
    radius = PetscSqrtReal(PetscPowRealInt(rg->sfactor*(ctx->b-ctx->a)/2.0,2)+PetscPowRealInt(rg->sfactor*(ctx->d-ctx->c)/2.0,2));
    for (i=0;i<n;i++) {
      zn[i] = (z[i]-center)/radius;
      tmp = 1.0; tmp2 = 1.0;
      for (j=0;j<n;j++) {
        tmp *= z[j];
        if (i != j) tmp2 *= z[j]-z[i];
      }
      w[i] = tmp/tmp2;
      max_w = PetscMax(PetscAbsScalar(w[i]),max_w);
    }
    for (i=0;i<n;i++) w[i] /= (PetscScalar)max_w;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGCheckInside_Interval(RG rg,PetscReal dx,PetscReal dy,PetscInt *inside)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (dx>ctx->a && dx<ctx->b) *inside = 1;
  else if (dx==ctx->a || dx==ctx->b) *inside = 0;
  else *inside = -1;
  if (*inside>=0) {
    if (dy>ctx->c && dy<ctx->d) ;
    else if (dy==ctx->c || dy==ctx->d) *inside = 0;
    else *inside = -1;
  }
  PetscFunctionReturn(0);
}

PetscErrorCode RGIsAxisymmetric_Interval(RG rg,PetscBool vertical,PetscBool *symm)
{
  RG_INTERVAL *ctx = (RG_INTERVAL*)rg->data;

  PetscFunctionBegin;
  if (vertical) *symm = (ctx->a == -ctx->b)? PETSC_TRUE: PETSC_FALSE;
  else *symm = (ctx->c == -ctx->d)? PETSC_TRUE: PETSC_FALSE;
  PetscFunctionReturn(0);
}

PetscErrorCode RGSetFromOptions_Interval(PetscOptionItems *PetscOptionsObject,RG rg)
{
  PetscErrorCode ierr;
  PetscBool      flg;
  PetscInt       k;
  PetscReal      array[4]={0,0,0,0};

  PetscFunctionBegin;
  ierr = PetscOptionsHead(PetscOptionsObject,"RG Interval Options");CHKERRQ(ierr);

    k = 4;
    ierr = PetscOptionsRealArray("-rg_interval_endpoints","Interval endpoints (two or four real values separated with a comma without spaces)","RGIntervalSetEndpoints",array,&k,&flg);CHKERRQ(ierr);
    if (flg) {
      if (k<2) SETERRQ(PetscObjectComm((PetscObject)rg),PETSC_ERR_ARG_SIZ,"Must pass at least two values in -rg_interval_endpoints (comma-separated without spaces)");
      ierr = RGIntervalSetEndpoints(rg,array[0],array[1],array[2],array[3]);CHKERRQ(ierr);
    }

  ierr = PetscOptionsTail();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode RGDestroy_Interval(RG rg)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscFree(rg->data);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)rg,"RGIntervalSetEndpoints_C",NULL);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)rg,"RGIntervalGetEndpoints_C",NULL);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

SLEPC_EXTERN PetscErrorCode RGCreate_Interval(RG rg)
{
  RG_INTERVAL    *interval;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscNewLog(rg,&interval);CHKERRQ(ierr);
  interval->a = -PETSC_MAX_REAL;
  interval->b = PETSC_MAX_REAL;
  interval->c = -PETSC_MAX_REAL;
  interval->d = PETSC_MAX_REAL;
  rg->data = (void*)interval;

  rg->ops->istrivial         = RGIsTrivial_Interval;
  rg->ops->computecontour    = RGComputeContour_Interval;
  rg->ops->computebbox       = RGComputeBoundingBox_Interval;
  rg->ops->computequadrature = RGComputeQuadrature_Interval;
  rg->ops->checkinside       = RGCheckInside_Interval;
  rg->ops->isaxisymmetric    = RGIsAxisymmetric_Interval;
  rg->ops->setfromoptions    = RGSetFromOptions_Interval;
  rg->ops->view              = RGView_Interval;
  rg->ops->destroy           = RGDestroy_Interval;
  ierr = PetscObjectComposeFunction((PetscObject)rg,"RGIntervalSetEndpoints_C",RGIntervalSetEndpoints_Interval);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)rg,"RGIntervalGetEndpoints_C",RGIntervalGetEndpoints_Interval);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

