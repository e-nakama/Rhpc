#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

// Include function prototypes defined in Rhpc.h
#include "common/Rhpc.h"

// External declarations of MPI-related functions defined in each source file
extern SEXP Rhpc_mpi_initialize(void);
extern SEXP Rhpc_gethandle(SEXP procs);
extern SEXP Rhpc_mpi_finalize(void);
extern SEXP Rhpc_number_of_worker(SEXP cl);
extern SEXP Rhpc_mpi_worker_call(SEXP cl, SEXP args, SEXP actioncode, SEXP usequote);
extern SEXP Rhpc_mpi_lapply_seq(SEXP cl, SEXP X, SEXP args, SEXP usequote);
extern SEXP Rhpc_mpi_lapply_LB(SEXP cl, SEXP X, SEXP args, SEXP usequote);

// List of C functions called from R
static const R_CallMethodDef CallEntries[] = {
    {"Rhpc_serialize",          (DL_FUNC) &Rhpc_serialize,          1},
    {"Rhpc_serialize_onlysize", (DL_FUNC) &Rhpc_serialize_onlysize, 1},
    {"Rhpc_serialize_norealloc",(DL_FUNC) &Rhpc_serialize_norealloc,1},
    {"Rhpc_unserialize",        (DL_FUNC) &Rhpc_unserialize,        1},
    {"Rhpc_enquote",            (DL_FUNC) &Rhpc_enquote,            1},
    {"Rhpc_splitList",          (DL_FUNC) &Rhpc_splitList,          2},
    {"Rhpc_mpi_initialize",     (DL_FUNC) &Rhpc_mpi_initialize,     0},
    {"Rhpc_gethandle",          (DL_FUNC) &Rhpc_gethandle,          1},
    {"Rhpc_mpi_finalize",       (DL_FUNC) &Rhpc_mpi_finalize,       0},
    {"Rhpc_number_of_worker",   (DL_FUNC) &Rhpc_number_of_worker,   1},
    {"Rhpc_mpi_worker_call",    (DL_FUNC) &Rhpc_mpi_worker_call,    4},
    {"Rhpc_mpi_lapply_seq",     (DL_FUNC) &Rhpc_mpi_lapply_seq,     4},
    {"Rhpc_mpi_lapply_LB",      (DL_FUNC) &Rhpc_mpi_lapply_LB,      4},
    {NULL, NULL, 0}
};

// Package initialization function
// Automatically called when R loads the package.
void attribute_visible R_init_Rhpc(DllInfo *dll)
{
#ifdef RHPC_ALTVEC
    // ALTREP class registration
    // Pass DllInfo pointer to Rhpc_init_serialize_altrep function.
    // This associates the ALTREP class with the Rhpc package shared library.
    Rhpc_init_serialize_altrep(dll);
#endif

    // Register C functions that can be called from R
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);

    // Disable dynamic symbol lookup
    // This ensures R only searches for functions registered in CallEntries,
    // preventing accidental access to unregistered C functions.
    R_useDynamicSymbols(dll, FALSE);
}
