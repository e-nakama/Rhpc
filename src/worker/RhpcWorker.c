/*
    Rhpc : R HPC environment
    Copyright (C) 2012-2018  Junji NAKANO and Ei-ji Nakama

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef WIN32
#include "../common/config.h"
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DLADDR
#  ifndef   _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  ifndef   __USE_GNU
#    define __USE_GNU
#  endif
#endif
#ifndef WIN32
#include <dlfcn.h>
#endif
#include <Rembedded.h>
#ifdef WIN32
#undef ERROR
#endif
#include <Rdefines.h>
#ifndef WIN32
#include <Rinterface.h>
#endif
#include <Rinternals.h>
#include <R_ext/Parse.h>

/* from Jeroen Ooms san! */
#include <Rversion.h>
#if R_VERSION >= R_Version(4,0,0)
#define R_Slave R_NoEcho
#endif

#define WORKER 1
#include <mpi.h>

/*
#if !defined(putenv)
extern int putenv(char *string);
#endif
*/

#include "../common/Rhpc.h"
#include "../common/Rhpc_ms.h"

static int initialize = 0;

static int MPI_rank=-1;
static int MPI_procs=-1;

static MPI_Comm RHPC_Comm;


static void Rhcp_worker_finalize(void)
{
  MPI_Finalize();
  Rf_endEmbeddedR(0);
  initialize=0;
}

extern Rboolean R_Interactive;  /* TRUE during interactive use*/
extern Rboolean R_Slave;        /* Run as a slave process */

static SEXP Rhpc_docall = NULL;

static void Rhpc_worker_init(void)
{
  /*
    int  errorOccurred=0;
    static char buf[4096];
  */
  MPI_Comm pcomm;
  int mpi_version, mpi_subversion;

#if defined(__ELF__)
  void *dlh = NULL;
  void *dls = NULL;
  int failmpilib=0;
# ifdef HAVE_DLADDR
    Dl_info info_MPI_Init;
    int rc ;
# endif
#endif

    /*
  sprintf(buf, "R_HOME=%s", (R_HOME));
  putenv(buf);
    */
    
  Rf_initEmbeddedR(MPI_argc, MPI_argv);
  R_Interactive        = FALSE;
  R_Slave              = TRUE;
  R_ReplDLLinit();
  initialize=1;

#if defined(__ELF__)
  if ( NULL != (dlh=dlopen(NULL, RTLD_NOW|RTLD_GLOBAL))){
    if(NULL != (dls = dlsym( dlh, "MPI_Init")))
      failmpilib = 0; /* success loaded MPI library */
    else
      failmpilib = 1; /* maybe can't loaded MPI library */
    dlclose(dlh);
  }
  
  if( failmpilib ){
#   ifdef HAVE_DLADDR
      /* maybe get beter soname */
      rc = mydladdr(MPI_Init, &info_MPI_Init);
      if (rc){
	Rprintf("reload mpi library %s\n", info_MPI_Init.dli_fname );
	if (!dlopen(info_MPI_Init.dli_fname, RTLD_GLOBAL | RTLD_LAZY)){
	  Rprintf("%s\n",dlerror());
	}
      }else{
	Rprintf("Can't get Information by dladdr of function MPI_Init,%s\n",
		dlerror());
      }
#   else
      Rprintf("Can't get Information by dlsym of function MPI_Init,%s\n",
	      dlerror());
#   endif
  }
#endif

  MPI_Get_version(&mpi_version, &mpi_subversion);
  if( mpi_version >= 2)
    _M(MPI_Init(NULL, NULL));
  else
    _M(MPI_Init((int *)&MPI_argc, (char ***)&MPI_argv));
  _M(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
  _M(MPI_Comm_set_errhandler(MPI_COMM_SELF, MPI_ERRORS_RETURN));

  pcomm = MPI_COMM_NULL;
  _M(MPI_Comm_get_parent(&pcomm));
  if (pcomm == MPI_COMM_NULL){
    RHPC_Comm=MPI_COMM_WORLD;
  }else{/* if worker */
    _M(MPI_Intercomm_merge( pcomm, 1, &RHPC_Comm ));
    _M(MPI_Comm_free(&pcomm));
    _M(MPI_Comm_set_errhandler(RHPC_Comm, MPI_ERRORS_RETURN));
  }
  
  _M(MPI_Comm_rank(RHPC_Comm, &MPI_rank));
  _M(MPI_Comm_size(RHPC_Comm, &MPI_procs));

  DPRINT("worker init\n");

  DPRINT("Worker:rank=%d:procs=%d\n", MPI_rank, MPI_procs);
  
  Rhpc_set_options(MPI_rank, MPI_procs, RHPC_Comm);

  /*
  {
    int errorOccurred;
    SEXP ret, l_ret;
    PROTECT(ret=R_NilValue);
    PROTECT(l_ret=R_NilValue);
    errorOccurred=0;
    l_ret=LCONS(install("library"),CONS(mkString("compiler"), R_NilValue));
    ret=R_tryEval(l_ret, R_GlobalEnv, &errorOccurred);
    UNPROTECT(2);
  }
  */

}

#include "RhpcWorker_LapplyLB.h"
#include "RhpcWorker_Lapplyseq.h"
#include "RhpcWorker_WorkerCall.h"

static void Rhpc_worker_main(void){
  int  cmd[CMDLINESZ];
  int  getcmd = 0;
  int  getsubcmd = 0;
  R_xlen_t cnt = 0;
  R_xlen_t mod = 0;
  int usequote = 0;
  SEXP cmdSexp, cmdexpr;
  ParseStatus status;
/*
  SEXP cmplang;
  int  errorOccurred=0;
*/

  push_policy();

  PROTECT(cmdSexp = allocVector(STRSXP, 1));
  SET_STRING_ELT(cmdSexp, 0, mkChar("function (fun, args)do.call(\"fun\", args)"));
  PROTECT( cmdexpr = R_ParseVector(cmdSexp, -1, &status, R_NilValue));
  PROTECT(Rhpc_docall=VECTOR_ELT(cmdexpr,0));
  do{
    SET_CMD(cmd, 0, 0, 0, 0, 0);
    _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, RHPC_Comm));
    GET_CMD(cmd, &getcmd, &getsubcmd, &cnt, &mod, &usequote);

    if      (getcmd==CMD_NAME_WORKERCALL_NORET)    Rhpc_worker_call(cmd,0);
    else if (getcmd==CMD_NAME_WORKERCALL_RET)      Rhpc_worker_call(cmd,1);
    else if (getcmd==CMD_NAME_WORKERCALL_EXPORT)   Rhpc_worker_call(cmd,2);
    else if (getcmd==CMD_NAME_LAPPLY_LB)           Rhpc_worker_lapply_LB(cmd);
    else if (getcmd==CMD_NAME_LAPPLY_SEQ)          Rhpc_worker_lapply_seq(cmd);
  }while(getcmd!=CMD_NAME_ENDL);
  UNPROTECT(3);
  /*
  UNPROTECT(4);
  */

  pop_policy();
}

int main(int argc, char	*argv[],char *arge[])
{
  Rhpc_worker_init();
  Rhpc_worker_main();
  Rhcp_worker_finalize();
  return(0);
}
