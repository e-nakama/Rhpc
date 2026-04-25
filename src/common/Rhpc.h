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

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if WIN32
#include <process.h>
#define getpid _getpid
#endif


#if 0
struct timeval ts,te;
#define GETTIME(x) gettimeofday(&x, NULL);
#define TMPRINT(fmt) Rprintf(fmt, (double)(te.tv_sec - ts.tv_sec) + ((double)te.
#define DPRINT(...) Rprintf(__VA_ARGS__)
#else
#define GETTIME(x)
#define TMPRINT(fmt)
__inline static void DPRINT(char *fmt,...)
{
  va_list arg;  
  if(0){
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
  }
  return;
}
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
__inline static int mydladdr( int(*mpiinit)(int*,char***), Dl_info *info){
  int rc;
  int (*__mydladdr)(int(*)(int*,char***), Dl_info *) = NULL;
  void *dlh =NULL;
  dlh = dlopen (NULL, RTLD_NOW|RTLD_GLOBAL);
  *(void **)(&__mydladdr)=dlsym(dlh, "dladdr");
  rc=__mydladdr(mpiinit,info);
  dlclose(dlh);
  return(rc);
}
#endif

__inline static int _M(int _X)
{
  int  _ec = _X;
  int  _el = 0;
  char _em[MPI_MAX_ERROR_STRING];
  if ( MPI_SUCCESS != _ec ){
    MPI_Error_string(_ec, _em, &_el);
    error(_em);
  }
  return _ec;
}

__inline static SEXP _CHK(SEXP _X)
{
  SEXP ret = _X;
  R_xlen_t errcnt=0;
  R_xlen_t i;
  char msg[512]="";
  char call[512]="";
  PROTECT(_X);
  for(i=0;i<xlength(_X);i++){
    SEXP target;
    SEXP klass;
    PROTECT(target = VECTOR_ELT(_X,i));
    PROTECT(klass = getAttrib(target, R_ClassSymbol));
    if(xlength(klass)==1 && 0==strcmp(CHAR(STRING_ELT(klass, 0)),"Rhpc-try-error")){
      if(errcnt==0){
	strncpy(msg,CHAR(STRING_ELT(VECTOR_ELT(target,0),0)),sizeof(msg)-1);
	msg[sizeof(msg)-1]=0;
	call[sizeof(call)-1]=0;
      }
      errcnt++;
    }
    UNPROTECT(2);
  }
  if(errcnt==1){
    error( "one remote produced an error: %s\n%s\n", msg, call);
    ret = R_UnboundValue;
  }else if (errcnt>1){
    error( "%d remote produced an errors; first error: %s\n%s\n", errcnt, msg, call);
    ret = R_UnboundValue;
  }
  UNPROTECT(1);
  return ret;
}


SEXP Rhpc_serialize(SEXP);
SEXP Rhpc_serialize_onlysize(SEXP);
SEXP Rhpc_serialize_norealloc(SEXP);
SEXP Rhpc_unserialize(SEXP);

#if !defined(WORKER) /* master only */
static char RHPC_WORKER_CMD[4096];
#endif
#define RHPC_SPLIT_SIZE (1UL<<30)
#define CMDLINESZ 5
#define RHPC_CTRL_TAG 0
#define TAGCAL(_x) ((int)_x + 1)

enum CMD_NAME{
  CMD_NAME_UNKNOWN,
  CMD_NAME_WORKERCALL_NORET,
  CMD_NAME_WORKERCALL_RET,
  CMD_NAME_WORKERCALL_EXPORT,
  CMD_NAME_LAPPLY_LB,
  CMD_NAME_LAPPLY_SEQ,
  CMD_NAME_MODE,
  CMD_NAME_SERIALIZE_MODE,
  CMD_NAME_ENDL
}; /* 0 - 8 */

enum SUBCMD_NAME{
  SUBCMD_NORMAL,
  SUBCMD_EXIT
};

enum POS_NAME{
  CMD_MAIN,
  CMD_SUB,
  CMD_CNT,
  CMD_MOD,
  CMD_QUO
};

__inline static void SET_CMD(int *cmd, int m, int s, R_xlen_t cnt, R_xlen_t mod, int usequote)
{
  cmd[CMD_MAIN]=m;
  cmd[CMD_SUB]=s;
  cmd[CMD_CNT]=(int)cnt;
  cmd[CMD_MOD]=(int)mod;
  cmd[CMD_QUO]=usequote;
}

__inline static void GET_CMD(int *cmd, int *m, int *s, R_xlen_t *cnt, R_xlen_t *mod, int *usequote)
{
  *m=cmd[CMD_MAIN];
  *s=cmd[CMD_SUB];
  *cnt=(R_xlen_t)cmd[CMD_CNT];
  *mod=(R_xlen_t)cmd[CMD_MOD];
  *usequote=cmd[CMD_QUO];
  DPRINT("CMD[%d]:%d:%d:%d:%d:%d\n", getpid(),cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]); 
} 

#if defined(WORKER)
static char *MPI_argv[]={"RhpcWorker",
			 "--gui none",
			 "--silent",
			 "--no-restore",
			 "--no-save",
			 "--no-readline"};
static int MPI_argc = sizeof(MPI_argv)/sizeof(char*);
#else
static char *MPI_argv[1]={"R"};
static int MPI_argc = 1;
#endif

#ifndef MYSCHED
int MYSCHED;
#endif
__inline static void push_policy(void)
{
#if (defined(_POSIX_PRIORITY_SCHEDULING) && defined(HAVE_SCHED_GETSCHEDULER))
  struct sched_param sp;
  int policy_max;
  MYSCHED = sched_getscheduler(0);
  if(-1 == (policy_max = sched_get_priority_max(SCHED_FIFO))){
    policy_max=0;
  }
  sp.sched_priority = policy_max;
  sched_setscheduler(0, SCHED_FIFO, &sp);
  /* Rprintf("SCHED=%d:max=%d\n", SCHED_FIFO, sp.sched_priority); */
#else
  MYSCHED = 1;
#endif /* _POSIX_PRIORITY_SCHEDULING */
}
__inline static void pop_policy(void)
{
#if (defined(_POSIX_PRIORITY_SCHEDULING) && defined(HAVE_SCHED_GETSCHEDULER))
  struct sched_param sp;
  int policy_max;
  if(-1 == (policy_max = sched_get_priority_max(MYSCHED))){
    policy_max=0;
  }
  sp.sched_priority = policy_max;
  sched_setscheduler(0, MYSCHED, &sp);
  /* Rprintf("SCHED=%d:max=%d\n", MYSCHED, sp.sched_priority); */
#else
  MYSCHED=0;
#endif /* _POSIX_PRIORITY_SCHEDULING */
}

#include <R_ext/Parse.h>
static void op_comm_free(SEXP com)
{
  void *ptr;
  if (TYPEOF(com) != EXTPTRSXP)
    error("not external pointer");
  ptr = R_ExternalPtrAddr(com);
  Free(ptr);
}

#define RHPC_MSG_BUF 256
__inline static void Rhpc_set_options(int rank, int procs, MPI_Comm ccomm)
{
  SEXP op_ex;
  SEXP op_nm;
  MPI_Comm *ptr;
  SEXP p_options=R_NilValue;
  SEXP p_rank =R_NilValue;
  SEXP p_procs=R_NilValue;
  SEXP p_ccomm=R_NilValue;
  SEXP p_fcomm=R_NilValue;
  int errorOccurred=0;

  /* Information */
  MPI_Fint f_comm=2; /* MPI_COMM_NULL */
  char host_name[MPI_MAX_PROCESSOR_NAME]="";
  int  host_name_len;

  PROTECT(p_options=install("options"));

  
  if(rank != -1){
    char *recvbuf = NULL;
    char sendbuf[RHPC_MSG_BUF];
    f_comm=MPI_Comm_c2f(ccomm);
    MPI_Get_processor_name(host_name, &host_name_len);

    if(rank==0) recvbuf=Calloc(RHPC_MSG_BUF*procs, char);

    snprintf(sendbuf, RHPC_MSG_BUF,
	     "%5d/%5d(%11d) : %-32.32s : %5d\n",
	     rank, procs, f_comm, host_name, getpid());   
    MPI_Gather(sendbuf, RHPC_MSG_BUF, MPI_BYTE,
	       recvbuf, RHPC_MSG_BUF, MPI_BYTE,
	       0, ccomm);
    if(rank==0 && recvbuf!=NULL){
      int i;
      Rprintf("%5s/%5s(%11s) : %-32.32s : %5s\n",
	      "rank", "procs", "f.comm","processor_name", "pid");
      for(i=0; i<procs; i++){
	Rprintf("%s", recvbuf + (i*RHPC_MSG_BUF)); 
      }
      Free(recvbuf);
    }

    PROTECT(p_rank = ScalarInteger(rank));
    PROTECT(p_procs= ScalarInteger(procs));
    ptr = Calloc(1,MPI_Comm);
    PROTECT(p_ccomm = R_MakeExternalPtr(ptr, R_NilValue, R_NilValue));
    *((MPI_Comm*)R_ExternalPtrAddr(p_ccomm)) = ccomm;
    R_RegisterCFinalizer(p_ccomm, op_comm_free);
    PROTECT(p_fcomm = ScalarInteger(f_comm));
    
  }else{
    PROTECT(p_rank);
    PROTECT(p_procs);
    PROTECT(p_ccomm);
    PROTECT(p_fcomm);
  }
  PROTECT(op_ex = LCONS(p_options,
			CONS(p_rank,
			     CONS(p_procs,
				  CONS(p_ccomm,
				       CONS(p_fcomm,
					    R_NilValue))))));
  PROTECT(op_nm = allocVector(STRSXP, 5));
  SET_STRING_ELT(op_nm,0,mkChar(""));
  SET_STRING_ELT(op_nm,1,mkChar("Rhpc.mpi.rank"));
  SET_STRING_ELT(op_nm,2,mkChar("Rhpc.mpi.procs"));
  SET_STRING_ELT(op_nm,3,mkChar("Rhpc.mpi.c.comm"));
  SET_STRING_ELT(op_nm,4,mkChar("Rhpc.mpi.f.comm"));
  setAttrib(op_ex, R_NamesSymbol, op_nm);
  R_tryEval(op_ex, R_GlobalEnv, &errorOccurred);

  UNPROTECT(7);
}


#define MYDEBUG 0


#if defined(MYDEBUG) & (MYDEBUG == 1) 
#include <stdio.h>
static void mydump(unsigned char* p, size_t sz)
{
  size_t i,j;
  char obuf[512];
  char buf[512];
  obuf[0]=0;
  printf("%d:%d\n",getpid(),(int)sz);
  for ( i = 0; i<sz;i+=16){
    buf[0]=0;
    for( j=0; j<16&& i+j<sz; j++){
      sprintf(buf+strlen(buf),"%02x ", p[i+j]);
    }
    sprintf(buf+strlen(buf),"\n");
    if(strcmp(buf,obuf)!=0){
      printf ("%d:%05lx: ", getpid(), i);
      printf ("%s",buf);
      strcpy(obuf,buf);
    }
  }
}

static void STR(SEXP _x)
{
  SEXP  _l,_f;
  int errorOccurred=0;
  _l = LCONS(install("str"),CONS(_x, R_NilValue));
  _l = LCONS(install("try"),CONS(_l, R_NilValue));
  PROTECT(_l);
  PROTECT(_f=R_tryEval(_l, R_GlobalEnv, &errorOccurred));
  UNPROTECT(2);
}

#endif
