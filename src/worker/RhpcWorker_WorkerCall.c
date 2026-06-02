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
#include "../common/Rhpc.h"
#include "RhpcWorker_WorkerCall.h"

void Rhpc_worker_call(int *cmd, int action)
{
  int  errorOccurred=0;
  int  get_cmd_main = 0, get_sub_cmd = 0, usequote = 0;
  R_xlen_t chunks_in = 0, remainder_in = 0, len_in, i;
  SEXP data, fun_arg, l_fun_arg, fun, arg, argnm, argq, lng, ret, l_out, out;

  GET_CMD(cmd, &get_cmd_main, &get_sub_cmd, &chunks_in, &remainder_in, &usequote);
  len_in = chunks_in * RHPC_SPLIT_SIZE + remainder_in;
  PROTECT(data = allocVector(RAWSXP, len_in));

  for (i = 0; i < chunks_in; i++) _M(MPI_Bcast(RAW(data) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, RHPC_Comm));
  if (remainder_in != 0) _M(MPI_Bcast(RAW(data) + RHPC_SPLIT_SIZE * chunks_in, (int)remainder_in, MPI_CHAR, 0, RHPC_Comm));

  PROTECT(fun_arg=Rhpc_unserialize(data));
  PROTECT(l_fun_arg=R_NilValue);

  if(action == 2){ /* Export */   
    PROTECT(fun = findFun(install("assign"), R_BaseEnv));
    PROTECT(arg = allocVector(VECSXP, 3));
    PROTECT(argnm = allocVector(STRSXP, 3));
    SET_STRING_ELT(argnm, 0, mkChar(""));
    SET_STRING_ELT(argnm, 1, mkChar(""));
    SET_STRING_ELT(argnm, 2, mkChar("envir"));
    SET_VECTOR_ELT(arg, 0, VECTOR_ELT(fun_arg, 0));
    SET_VECTOR_ELT(arg, 1, VECTOR_ELT(fun_arg, 1));
    SET_VECTOR_ELT(arg, 2, R_GlobalEnv);
    setAttrib(arg, R_NamesSymbol, argnm);
  }else{
    fun = VECTOR_ELT(fun_arg, 0);
    if (TYPEOF(fun) == STRSXP && xlength(fun) == 1) {
      SEXP find_fun = findFun(install(CHAR(STRING_ELT(fun, 0))), R_GlobalEnv);
      if(find_fun != R_UnboundValue) fun = find_fun;
    } else if (TYPEOF(fun) == SYMSXP){
      SEXP find_fun = findFun(fun, R_GlobalEnv);
      if(find_fun != R_UnboundValue) fun = find_fun;
    }
    PROTECT(fun);
    PROTECT(arg = VECTOR_ELT(fun_arg, 1));
    PROTECT(argnm=R_NilValue);
  }
  
  if (usequote) PROTECT(argq=Rhpc_enquote(arg));
  else PROTECT(argq=arg);

  errorOccurred=0;
  PROTECT(lng = LCONS(Rhpc_docall, CONS(fun, CONS(argq, R_NilValue))));

  ret=R_tryEval(lng, R_GlobalEnv, &errorOccurred);
    
  if (action == 0) {
    UNPROTECT(8);
    return;
  }
  
  if(errorOccurred){
    SEXP eclass, elist, elist_label;
    eclass = mkString("Rhpc-try-error");
    elist = allocVector(VECSXP, 2);
    SET_VECTOR_ELT(elist, 0, mkString(R_curErrorBuf()));
    SET_VECTOR_ELT(elist, 1, lng);
    elist_label = allocVector(STRSXP, 2);
    SET_STRING_ELT(elist_label, 0, mkChar("message"));
    SET_STRING_ELT(elist_label, 1, mkChar("call"));
    setAttrib(elist, R_NamesSymbol, elist_label);
    setAttrib(elist, R_ClassSymbol, eclass);
    ret=elist;
  }
  PROTECT(ret);
  PROTECT(l_out=R_NilValue);
  PROTECT(out=Rhpc_serialize_norealloc(ret));

  {
    int cmd_ret[CMDLINESZ], dummy_cmd[CMDLINESZ];
    R_xlen_t sz_out = xlength(out);
    R_xlen_t chunks_out, remainder_out;
    Rhpc_get_chunks(sz_out, &chunks_out, &remainder_out);

    int total_send_calls = (int)chunks_out + (remainder_out ? 1 : 0);
    MPI_Request *request = R_Calloc(total_send_calls, MPI_Request);
    MPI_Status *status = R_Calloc(total_send_calls, MPI_Status);
    int call_idx = 0;

    SET_CMD(cmd_ret, get_cmd_main, SUBCMD_NORMAL, chunks_out, remainder_out, usequote);
    _M(MPI_Gather(cmd_ret, CMDLINESZ, MPI_INT, dummy_cmd, CMDLINESZ, MPI_INT, 0, RHPC_Comm));

    for (i = 0; i < chunks_out; i++) {
      _M(MPI_Isend(RAW(out) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &request[call_idx++]));
    }
    if (remainder_out != 0) {
      _M(MPI_Isend(RAW(out) + RHPC_SPLIT_SIZE * chunks_out, (int)remainder_out, MPI_CHAR, 0, TAGCAL(chunks_out), RHPC_Comm, &request[call_idx++]));
    }
    _M(MPI_Waitall(call_idx, request, status));

    R_Free(request);
    R_Free(status);
  }
  UNPROTECT(11);

  return;
}
