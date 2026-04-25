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
#include "common/config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
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
#include <sched.h>
#endif

#include <mpi.h>

#include <R.h>
#include <Rinternals.h>
#include "common/Rhpc.h"

#ifdef WIN32
#include "common/fakemaster.h"
#include <windows.h>
#endif

static int initialize=0;
static int finalize=0;

static int MPI_rank = 0;
static int MPI_procs = 0;

static MPI_Comm RHPC_Comm = MPI_COMM_NULL;

/* fake master */
#ifdef WIN32
static void setmpienv(char *buf){
  char *b = buf;
  Rprintf("Import MPI environment variables.\n");
  Rprintf("---------------------------------\n");
  while(*b){
    Rprintf(" %s\n",b);
    _putenv(b);
    b+=strlen(b)+1;
  }
  Rprintf("---------------------------------\n");
}
static void fakemastercmd(char *buf, size_t buf_sz, char *pipename)
{
  char fakemaster[FAKE_PATH_MAX];
  char mpiexeccmd[FAKE_PATH_MAX];
  int  errorOccurred=0;
  SEXP ret;
  SEXP cmdSexp, cmdexpr;
  ParseStatus status;
  
  PROTECT(cmdSexp = allocVector(STRSXP, 1));
#ifdef WIN64
  SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('fakemaster64.exe',package='Rhpc')"));
#else /* WIN32 */
  SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('fakemaster32.exe',package='Rhpc')"));
#endif
  PROTECT( cmdexpr = R_ParseVector(cmdSexp, -1, &status, R_NilValue));
  ret=R_tryEval(VECTOR_ELT(cmdexpr,0), R_GlobalEnv, &errorOccurred);
  strncpy(fakemaster, CHAR(STRING_ELT(ret,0)), sizeof(fakemaster)-1);
  
  ret=GetOption1(install("Rhpc.mpiexec"));
  if (TYPEOF(ret) == STRSXP)
    strncpy(mpiexeccmd, CHAR(STRING_ELT(ret,0)), sizeof(mpiexeccmd)-1);    
  else{
    SEXP op_ex;
    SEXP op_nm;
    SEXP op_opt;
    SEXP op_mpi;
    strncpy(mpiexeccmd, FAKE_DEFAULT_MPIEXEC,    sizeof(mpiexeccmd)-1);
    PROTECT(op_opt = install("options"));
    PROTECT(op_mpi = ScalarString(mkChar(FAKE_DEFAULT_MPIEXEC)));
    PROTECT(op_ex = LCONS(op_opt, CONS(op_mpi, R_NilValue)));
    PROTECT(op_nm = allocVector(STRSXP, 2));
    SET_STRING_ELT(op_nm,0,mkChar(""));
    SET_STRING_ELT(op_nm,1,mkChar("Rhpc.mpiexec"));
    setAttrib(op_ex, R_NamesSymbol, op_nm);
    R_tryEval(op_ex, R_GlobalEnv, &errorOccurred);
    UNPROTECT(4);    
  }
  snprintf(buf, buf_sz, "%s \"%s\" %s", mpiexeccmd, fakemaster, pipename); 
  //MessageBox(NULL,buf,"CMD",MB_OK);
  UNPROTECT(2);
}

static HANDLE hP = NULL;


static int cheatMPImaster(void)
{
  PROCESS_INFORMATION fakemaster_pi = { 0 }; 
  STARTUPINFO fakemaster_si = { sizeof(STARTUPINFO) };
  DWORD dwNumberOfBytesRead;

  char pipename[FAKE_PATH_MAX];
  char cmd[FAKE_PATH_MAX];
  char buf[FAKE_BUF_SZ];
  int pid = getpid();
 
  if(NULL==getenv("MSMPI_BIN")){
    warning("MS-MPI is required to run Rhpc on Windows.\n"
	    "please install MS-MPI.\n"
	    "(https://msdn.microsoft.com/library/windows/desktop/bb524831)\n"
	    "requires installation of MSMpiSetup.exe\n"
	    "require install msmpisdk.msi if you wanna build Rhpc");
    return(1);
  }
  if(NULL!=getenv("PMI_RANK")) return 0;
    
  Rprintf("Since this process was not started under mpiexec,\nstart up the cheat MPI master process:D\n");
  
  snprintf(pipename, sizeof(pipename), FAKE_PIPENAMEFMT, pid);
  hP = CreateNamedPipe(pipename,
    PIPE_ACCESS_DUPLEX|FILE_FLAG_WRITE_THROUGH,
		       PIPE_TYPE_BYTE,
		       1,
		       FAKE_BUF_SZ,
		       FAKE_BUF_SZ,
		       FAKE_WAIT_TIME,
		       NULL);
  if(hP == INVALID_HANDLE_VALUE){
    warning("Invalid handle value : named pipe %s", pipename);
    return 1;
  }

  /* mpiexec */
  fakemastercmd(cmd,sizeof(cmd)-1, pipename);
  fakemaster_si.wShowWindow=SW_HIDE;
  if(0 == CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE|NORMAL_PRIORITY_CLASS, NULL, NULL, &fakemaster_si, &fakemaster_pi)){
    char *msg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL,
		  GetLastError(),
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR) &msg,
		  0,
		  NULL
		  );
    warning(msg);
    LocalFree(msg);
    CloseHandle(hP);
    return 1;
  }
  
  /* wait for fakemaster */
  if(!ConnectNamedPipe(hP, NULL)){
    CloseHandle(hP);
    warning("connection from fakemaster seems to have failed.");
    return 1;
  }

  /* read MPI environment variable */
  memset(buf,0,sizeof(buf));
  if (!ReadFile(hP, buf, sizeof(buf), &dwNumberOfBytesRead, NULL)) {
    char *msg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL,
		  GetLastError(),
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		  (LPTSTR) &msg,
		  0,
		  NULL
		  );
    warning(msg);
    LocalFree(msg);
    CloseHandle(hP);
    return 1;
  }

  setmpienv(buf); 
  return 0;
}
#endif


SEXP Rhpc_mpi_initialize(void)
{
  int *mpi_argc = (int *)&MPI_argc;
  char ***mpi_argv= (char ***)MPI_argv;
  int mpi_version = 0;
  int mpi_subversion = 0;

#if defined(__ELF__)
  void *dlh = NULL;
  void *dls = NULL;
  int failmpilib=0;
# ifdef HAVE_DLADDR
    Dl_info info_MPI_Init;
    int rc ;
# endif
#endif

#ifdef WIN32
    if(cheatMPImaster()){
      warning("Rhpc can't initialize.");
      return(R_NilValue);
    }
#endif
    
  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(initialize){
    warning("Rhpc were already initialized.");
    return(R_NilValue);
  }


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
      if(rc){
	Rprintf("reload mpi library %s\n", info_MPI_Init.dli_fname );
	if(!dlopen(info_MPI_Init.dli_fname, RTLD_GLOBAL | RTLD_LAZY)){
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
  if ( mpi_version >= 2){
    mpi_argc=NULL;
    mpi_argv=NULL;
  }
  _M(MPI_Init(mpi_argc, mpi_argv));
  _M(MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN));
  _M(MPI_Comm_set_errhandler(MPI_COMM_SELF, MPI_ERRORS_RETURN));
  _M(MPI_Comm_rank(MPI_COMM_WORLD, &MPI_rank));
  _M(MPI_Comm_size(MPI_COMM_WORLD, &MPI_procs));
  DPRINT("Rhpc_initialize : rank:%d size:%d\n", MPI_rank, MPI_procs);
  
  RHPC_Comm = MPI_COMM_WORLD;
  Rhpc_set_options( MPI_rank, MPI_procs,RHPC_Comm);

  if (MPI_rank == 0){ /* Master : get RhpcSpawn path*/
    int  errorOccurred=0;
    SEXP ret;
    SEXP cmdSexp, cmdexpr;
    ParseStatus status;

    PROTECT(cmdSexp = allocVector(STRSXP, 1));
#ifdef WIN64
    SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('RhpcSpawnWin64.cmd',package='Rhpc')"));
#elif  WIN32
    SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('RhpcSpawnWin32.cmd',package='Rhpc')"));
#else
    SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('RhpcSpawn',package='Rhpc')"));
#endif
    PROTECT( cmdexpr = R_ParseVector(cmdSexp, -1, &status, R_NilValue));
    ret=R_tryEval(VECTOR_ELT(cmdexpr,0), R_GlobalEnv, &errorOccurred);
    strncpy(RHPC_WORKER_CMD, CHAR(STRING_ELT(ret,0)), sizeof(RHPC_WORKER_CMD)-1);
    UNPROTECT(2);
  }

  initialize = 1;
  return(R_NilValue);
}

static void comm_free(SEXP com)
{
  void *ptr;
  if (TYPEOF(com) != EXTPTRSXP)
    error("not external pointer");
  ptr = R_ExternalPtrAddr(com);
  Free(ptr);
}

#define SXP2COMM(x) *((MPI_Comm*)R_ExternalPtrAddr(x))
#define SXP2COMMP(x) ((MPI_Comm*)R_ExternalPtrAddr(x))

SEXP Rhpc_gethandle(SEXP procs)
{
  int num_procs;
  MPI_Comm *ptr;
  SEXP com;
  int num;
  MPI_Comm pcomm;
  char **argv=MPI_ARGV_NULL;
#ifdef WIN32
  char *spawn_argv[3];
  char r_home[FAKE_PATH_MAX];
  char target_path[FAKE_PATH_MAX];
#endif
  
  if (RHPC_Comm == MPI_COMM_NULL){
    error("Rhpc_initialize is not called.");
    return(R_NilValue);
  }
  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(!initialize){
    warning("Rhpc not initialized.");
    return(R_NilValue);
  }
 
  num_procs = INTEGER (procs)[0];

  ptr = Calloc(1,MPI_Comm);
  PROTECT(com = R_MakeExternalPtr(ptr, R_NilValue, R_NilValue));
  R_RegisterCFinalizer(com, comm_free);
  SXP2COMM(com) = RHPC_Comm;

  if (num_procs == NA_INTEGER){/* use mpirun */
    _M(MPI_Comm_size(SXP2COMM(com), &num));
    Rprintf("Detected communication size %d\n", num);
    if( num > 1 ){
      if ( num_procs > 0){
	warning("blind procs argument, return of MPI_COMM_WORLD");
      }
    }else{
      if ( num == 1){
	warning("only current master process. not found worker process.");
      }
      SXP2COMM(com)=MPI_COMM_NULL;
      warning("please pecifies the number of processes in mpirun or mpiexec, or provide a number of process to spawn");
    }
    UNPROTECT(1);
    return(com);
  }else{ /* spawn */
    if(num_procs < 1){
      warning("you need positive number of procs argument");
      UNPROTECT(1);
      return(com);
    }
    _M(MPI_Comm_size(SXP2COMM(com), &num));
    if(num > 1){ 
      warning("blind procs argument, return of last communicator");
      UNPROTECT(1);
      return(com);
    }
  }

#ifdef WIN32
  strncpy(r_home, getenv("R_HOME"), sizeof(r_home)-1); 
  if(1){
    int  errorOccurred=0;
    SEXP ret;
    SEXP cmdSexp, cmdexpr;
    ParseStatus status;
    
    PROTECT(cmdSexp = allocVector(STRSXP, 1));
#ifdef WIN64
    SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('RhpcWorker64.exe',package='Rhpc')"));
#else
    SET_STRING_ELT(cmdSexp, 0, mkChar("system.file('RhpcWorker32.exe',package='Rhpc')"));
#endif
    PROTECT( cmdexpr = R_ParseVector(cmdSexp, -1, &status, R_NilValue));
    ret=R_tryEval(VECTOR_ELT(cmdexpr,0), R_GlobalEnv, &errorOccurred);
    strncpy(target_path, CHAR(STRING_ELT(ret,0)), sizeof(target_path)-1);
    UNPROTECT(2);
    spawn_argv[0]=r_home;
    spawn_argv[1]=target_path;
    spawn_argv[2]=NULL;
    argv=spawn_argv;
  }
#endif
  
  _M(MPI_Comm_spawn(RHPC_WORKER_CMD, argv, num_procs,
		    MPI_INFO_NULL, 0, MPI_COMM_SELF, &pcomm,  
		    MPI_ERRCODES_IGNORE));
  _M(MPI_Intercomm_merge( pcomm, 0, SXP2COMMP(com)));
  _M(MPI_Comm_free( &pcomm ));
  _M(MPI_Comm_size(SXP2COMM(com), &num));
  RHPC_Comm = SXP2COMM(com); /* rewrite RHPC_Comm */
  _M(MPI_Comm_set_errhandler(RHPC_Comm, MPI_ERRORS_RETURN));
  _M(MPI_Comm_rank(RHPC_Comm, &MPI_rank));
  _M(MPI_Comm_size(RHPC_Comm, &MPI_procs));
  DPRINT("Rhpc_getHandle(MPI_Comm_spawn : rank:%d size:%d\n", MPI_rank, MPI_procs);
  Rhpc_set_options( MPI_rank, MPI_procs,RHPC_Comm);
  UNPROTECT(1);
  return(com);
}

SEXP Rhpc_mpi_finalize(void)
{
  int cmd[CMDLINESZ];

  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(!initialize){
    warning("Rhpc not initialized.");
    return(R_NilValue);
  }

  SET_CMD(cmd, CMD_NAME_ENDL, SUBCMD_NORMAL, 0, 0, 0);
  _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, RHPC_Comm));
  MPI_Finalize();
  finalize =1;
  Rhpc_set_options( -1, -1, MPI_COMM_NULL);

#ifdef WIN32
  /* close fakemaster */
  if(hP!=NULL&&hP!=INVALID_HANDLE_VALUE){
    char buf[FAKE_BUF_SZ];
    strncpy(buf, "EOT", sizeof(buf)-1);
    DWORD dwNumberOfBytesWritten;

    if(0==WriteFile(hP,
		    buf,
		    sizeof(buf),
		    &dwNumberOfBytesWritten, NULL)){
      CloseHandle(hP);
      return(R_NilValue);
    }
    FlushFileBuffers(hP);
    CloseHandle(hP);
  }
#endif
  return(R_NilValue);
}

SEXP Rhpc_number_of_worker(SEXP cl)
{
  MPI_Comm comm;
  SEXP num;
  int procs;

  if(TYPEOF(cl)!=EXTPTRSXP){
    error("it's not MPI_Comm external pointer\n");
  }
  comm = SXP2COMM(cl);

  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(!initialize){
    warning("Rhpc not initialized.");
    return(R_NilValue);
  }

  PROTECT(num=allocVector(INTSXP,1));
  _M(MPI_Comm_size(comm, &procs));
  INTEGER(num)[0]=procs-1;
  UNPROTECT(1);
  return(num);
}

#include "common/Rhpc_ms.h"
#include "RhpcMPIlapplyLB.h"
#include "RhpcMPIlapplyseq.h"
#include "RhpcMPIWorkerCall.h"


