/*
    Rhpc : R HPC environment
    Copyright (C) 2012-2026 Ei-ji Nakama and Junji NAKANO

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    any later version.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef WIN32
#include "../common/config.h"
#else
#include <windows.h>
#endif

#define WORKER 1
#include <mpi.h>
#include <R.h>
#include <Rinternals.h>
#include "../common/Rhpc.h"
#include "RhpcWorker_WorkerCall.h"

void Rhpc_worker_call(int *cmd, int action)
{
  int  errorOccurred=0;
  int  getcmd = 0, getsubcmd = 0, usequote = 0;
  R_xlen_t cnti = 0, modi = 0, leni, i;
  SEXP data, fun_arg, l_fun_arg, fun, arg, argnm, argq, lng, ret, l_out, out;

  GET_CMD(cmd, &getcmd, &getsubcmd, &cnti, &modi, &usequote);
  leni = cnti * RHPC_SPLIT_SIZE + modi;
  PROTECT(data = allocVector(RAWSXP,leni));

  for(i = 0; i < cnti;i++) _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, RHPC_Comm));
  if( modi !=0 ) _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*cnti, (int)modi, MPI_CHAR, 0, RHPC_Comm));

  PROTECT(fun_arg=Rhpc_unserialize(data));
  PROTECT(l_fun_arg=R_NilValue);

  if(action == 2){ /* Export */   
    PROTECT(fun = findVar(install("assign"),R_BaseEnv));
    PROTECT(arg = allocVector(VECSXP,3));
    PROTECT(argnm = allocVector(STRSXP,3));
    SET_STRING_ELT(argnm,0,mkChar(""));
    SET_STRING_ELT(argnm,1,mkChar(""));
    SET_STRING_ELT(argnm,2,mkChar("envir"));
    SET_VECTOR_ELT(arg,0,VECTOR_ELT(fun_arg,0));
    SET_VECTOR_ELT(arg,1,VECTOR_ELT(fun_arg,1));
    SET_VECTOR_ELT(arg,2,R_GlobalEnv);
    setAttrib(arg, R_NamesSymbol, argnm);
  }else{
    fun = VECTOR_ELT(fun_arg,0);
    if ( TYPEOF(fun) == STRSXP && xlength(fun)==1 ){
      SEXP find_fun = findVar(install(CHAR(STRING_ELT(fun,0))), R_GlobalEnv);
      if(find_fun != R_UnboundValue) fun = find_fun;
    } else if (TYPEOF(fun) == SYMSXP){
      SEXP find_fun = findVar(fun, R_GlobalEnv);
      if(find_fun != R_UnboundValue) fun = find_fun;
    }
    PROTECT(fun);
    PROTECT(arg = VECTOR_ELT(fun_arg,1));
    PROTECT(argnm=R_NilValue);
  }
  
  if (usequote) PROTECT(argq=Rhpc_enquote(arg));
  else PROTECT(argq=arg);

  errorOccurred=0;
  if (usequote) PROTECT(lng = LCONS(Rhpc_docall, CONS(fun,CONS(argq, R_NilValue))));
  else PROTECT(lng = LCONS(install("do.call"), CONS(fun, CONS(argq, R_NilValue))));

  ret=R_tryEval(lng, R_GlobalEnv, &errorOccurred);
    
  if(action == 0){
    UNPROTECT(8);
    return;
  }
  
  if(errorOccurred){
    SEXP eclass, elist, elist_label;
    eclass= mkString("Rhpc-try-error");
    elist=allocVector(VECSXP,2);
    SET_VECTOR_ELT(elist,0,mkString(R_curErrorBuf()));
    SET_VECTOR_ELT(elist,1,lng);
    elist_label=allocVector(STRSXP,2);
    SET_STRING_ELT(elist_label,0,mkChar("message"));
    SET_STRING_ELT(elist_label,1,mkChar("call"));
    setAttrib(elist, R_NamesSymbol, elist_label);
    setAttrib(elist, R_ClassSymbol, eclass);
    ret=elist;
  }
  PROTECT(ret);
  PROTECT(l_out=R_NilValue);
  PROTECT(out=Rhpc_serialize_norealloc(ret));

  {
    int cmd_ret[CMDLINESZ], dummy_cmd[CMDLINESZ];
    R_xlen_t sz = xlength(out);
    R_xlen_t cnto = sz / RHPC_SPLIT_SIZE;
    R_xlen_t modo = sz % RHPC_SPLIT_SIZE;
    int reqcnt = cnto+((modo)?1:0);

    MPI_Request *request=R_Calloc(reqcnt,MPI_Request);
    MPI_Status *status =R_Calloc(reqcnt,MPI_Status);
    int calls=0;

    SET_CMD(cmd_ret, getcmd, SUBCMD_NORMAL, cnto, modo, usequote );
    _M(MPI_Gather(cmd_ret, CMDLINESZ, MPI_INT, dummy_cmd, CMDLINESZ, MPI_INT, 0, RHPC_Comm));

    for( i = 0 ; i< cnto ; i++){
      _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &request[calls++]));
    }
    if ( modo != 0 ){
      _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*cnto, (int)modo, MPI_CHAR, 0, TAGCAL(cnto), RHPC_Comm, &request[calls++]));
    }
    _M(MPI_Waitall(calls, request, status));

    R_Free(request);
    R_Free(status);
  }
  UNPROTECT(11);

  return;
}
