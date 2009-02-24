# --------------------------------------------------------------------

class STType(object):
    """
    ST types
    """
    SHELL  = STSHELL
    SHIFT  = STSHIFT
    SINV   = STSINV
    CAYLEY = STCAYLEY
    FOLD   = STFOLD

class STMatMode(object):
    """
    ST matrix mode
    """
    COPY    = STMATMODE_COPY
    INPLACE = STMATMODE_INPLACE
    SHELL   = STMATMODE_SHELL

class STMatStructure(object):
    # native
    SAME_NONZERO_PATTERN      = MAT_SAME_NONZERO_PATTERN
    DIFFERENT_NONZERO_PATTERN = MAT_DIFFERENT_NONZERO_PATTERN
    SUBSET_NONZERO_PATTERN    = MAT_SUBSET_NONZERO_PATTERN
    # aliases
    SAME      = SAME_NZ      = SAME_NONZERO_PATTERN
    SUBSET    = SUBSET_NZ    = SUBSET_NONZERO_PATTERN
    DIFFERENT = DIFFERENT_NZ = DIFFERENT_NONZERO_PATTERN

# --------------------------------------------------------------------

cdef class ST(Object):

    """
    ST
    """

    Type         = STType
    MatMode      = STMatMode
    MatStructure = STMatStructure

    def __cinit__(self):
        self.obj = <PetscObject*> &self.st
        self.st = NULL

    def view(self, Viewer viewer=None):
        cdef PetscViewer vwr = NULL
        if viewer is not None: vwr = viewer.vwr
        CHKERR( STView(self.st, vwr) )

    def destroy(self):
        CHKERR( STDestroy(self.st) )
        self.st = NULL
        return self

    def create(self, comm=None):
        cdef MPI_Comm ccomm = def_Comm(comm, SLEPC_COMM_DEFAULT())
        cdef SlepcST newst = NULL
        CHKERR( STCreate(ccomm, &newst) )
        self.dec_ref(); self.st = newst
        return self

    def setType(self, st_type):
        CHKERR( STSetType(self.st, str2cp(st_type)) )

    def getType(self):
        cdef SlepcSTType st_type = NULL
        CHKERR( STGetType(self.st, &st_type) )
        return cp2str(st_type)

    def setOptionsPrefix(self, prefix):
        CHKERR( STSetOptionsPrefix(self.st, str2cp(prefix)) )

    def getOptionsPrefix(self):
        cdef const_char_p prefix = NULL
        CHKERR( STGetOptionsPrefix(self.st, &prefix) )
        return cp2str(prefix)

    def setFromOptions(self):
        CHKERR( STSetFromOptions(self.st) )

    #

    def setShift(self, shift):
        cdef PetscScalar sval = asScalar(shift)
        CHKERR( STSetShift(self.st, sval) )

    def getShift(self):
        cdef PetscScalar sval = 0
        CHKERR( STGetShift(self.st, &sval) )
        return toScalar(sval)

    def setMatMode(self, mode):
        cdef SlepcSTMatMode val = mode
        CHKERR( STSetMatMode(self.st, val) )

    def getMatMode(self):
        cdef SlepcSTMatMode val = STMATMODE_INPLACE
        CHKERR( STGetMatMode(self.st, &val) )
        return val

    def setOperators(self, Mat A not None, Mat B=None):
        cdef PetscMat Bmat = NULL
        if B is not None: Bmat = B.mat
        CHKERR( STSetOperators(self.st, A.mat, Bmat) )

    def getOperators(self):
        cdef Mat A = Mat()
        cdef Mat B = Mat()
        CHKERR( STGetOperators(self.st, &A.mat, &B.mat) )
        A.inc_ref(); B.inc_ref(); return (A, B)

    def setMatStructure(self, structure):
        cdef PetscMatStructure val = structure
        CHKERR( STSetMatStructure(self.st, val) )

    def setKSP(self, KSP ksp not None):
        CHKERR( STSetKSP(self.st, ksp.ksp) )

    def getKSP(self):
        cdef KSP ksp = KSP()
        CHKERR( STGetKSP(self.st, &ksp.ksp) )
        ksp.inc_ref(); return ksp

    #

    def setUp(self):
        CHKERR( STSetUp(self.st) )

    def apply(self, Vec x not None, Vec y not None):
        CHKERR( STApply(self.st, x.vec, y.vec) )

    def applyTranspose(self, Vec x not None, Vec y not None):
        CHKERR( STApplyTranspose(self.st, x.vec, y.vec) )

    #

    def setCayleyAntishift(self, tau):
        cdef PetscScalar sval = asScalar(tau)
        CHKERR( STCayleySetAntishift(self.st, sval) )

    def setFoldLeftSide(self, left):
        cdef PetscTruth tval = PETSC_FALSE
        if left: tval = PETSC_TRUE
        CHKERR( STFoldSetLeftSide(self.st, tval) )

    #

    property shift:
        def __get__(self):
            return self.getShift()
        def __set__(self, value):
            self.setShift(value)

    property mat_mode:
        def __get__(self):
            return self.getMatMode()
        def __set__(self, value):
            self.setMatMode(value)

    property ksp:
        def __get__(self):
            return self.getKSP()
        def __set__(self, value):
            self.setKSP(value)

# --------------------------------------------------------------------

del STType
del STMatMode
del STMatStructure

# --------------------------------------------------------------------
