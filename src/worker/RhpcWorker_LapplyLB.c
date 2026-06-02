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
#include "RhpcWorker_LapplyLB.h"

void Rhpc_worker_lapply_LB(int *cmd)
{
  int  errorOccurred=0;
  int  get_cmd_main = 0, get_sub_cmd = 0, usequote = 0;
  R_xlen_t chunks_in = 0, remainder_in = 0, len_in, i;
  SEXP data, fun_arg, l_fun_arg, fun, arg, argq;

  GET_CMD(cmd, &get_cmd_main, &get_sub_cmd, &chunks_in, &remainder_in, &usequote);
  len_in = chunks_in * RHPC_SPLIT_SIZE + remainder_in;
  PROTECT(data = allocVector(RAWSXP, len_in));

  for (i = 0; i < chunks_in; i++) _M(MPI_Bcast(RAW(data) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, RHPC_Comm));
  if (remainder_in != 0) _M(MPI_Bcast(RAW(data) + RHPC_SPLIT_SIZE * chunks_in, (int)remainder_in, MPI_CHAR, 0, RHPC_Comm));

  PROTECT(fun_arg=Rhpc_unserialize(data));
  PROTECT(l_fun_arg=R_NilValue);

  fun = VECTOR_ELT(fun_arg, 0);
  if (TYPEOF(fun) == STRSXP && xlength(fun) == 1) {
    SEXP find_fun = findFun(install(CHAR(STRING_ELT(fun, 0))), R_GlobalEnv);
    if (find_fun != R_UnboundValue) fun = find_fun;
  } else if (TYPEOF(fun) == SYMSXP) {
    SEXP find_fun = findFun(fun, R_GlobalEnv);
    if (find_fun != R_UnboundValue) fun = find_fun;
  }
  PROTECT(fun);
  PROTECT(arg = VECTOR_ELT(fun_arg, 1));
  
  if(usequote) PROTECT(argq=Rhpc_enquote(arg));
  else PROTECT(argq=arg);

  for(;1;){
    int cmdx[CMDLINESZ], getx = 0, getsubx = 0;
    R_xlen_t cntx = 0, modx = 0, lenx = 0;
    MPI_Status stat;
    SEXP datax, tag_X, tag_X_l, tag, X, argw, namesymbol, names;
    R_xlen_t argw_len;
    SEXP lng,ret, tag_ret, l_out, out;

    _M(MPI_Recv(cmdx, CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &stat));
    GET_CMD(cmdx, &getx, &getsubx, &cntx, &modx, &usequote);

    if (getsubx == SUBCMD_EXIT) {
      UNPROTECT(6);
      return;
    }
    
    lenx = RHPC_SPLIT_SIZE * cntx + modx;
    PROTECT(datax = allocVector(RAWSXP, lenx));
    
    int total_recv_calls = (int)cntx + (modx ? 1 : 0);
    int call_idx = 0;
    MPI_Request *requestx = R_Calloc(total_recv_calls, MPI_Request);
    MPI_Status *statusx = R_Calloc(total_recv_calls, MPI_Status);
      
    for (i = 0; i < cntx; i++) _M(MPI_Irecv(RAW(datax) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &requestx[call_idx++]));
    if (modx != 0) _M(MPI_Irecv(RAW(datax) + RHPC_SPLIT_SIZE * cntx, (int)modx, MPI_CHAR, 0, TAGCAL(cntx), RHPC_Comm, &requestx[call_idx++]));

    _M(MPI_Waitall(call_idx, requestx, statusx));
    R_Free(requestx); R_Free(statusx);

    PROTECT(tag_X_l=R_NilValue);
    PROTECT(tag_X = Rhpc_unserialize(datax));
    PROTECT(tag = VECTOR_ELT(tag_X, 0));
    PROTECT(X = VECTOR_ELT(tag_X, 1));

    PROTECT(names = getAttrib(argq, R_NamesSymbol));
    PROTECT(namesymbol = allocVector(STRSXP, xlength(argq) + 1));
    argw = allocVector(VECSXP, xlength(argq) + 1);
    argw_len = xlength(argw);
    for (i = 0; i < argw_len; i++) {
      if (i) SET_VECTOR_ELT(argw, i, VECTOR_ELT(argq, i - 1));
      else {
        if (usequote) SET_VECTOR_ELT(argw, i, LCONS(install("quote"), CONS(X, R_NilValue)));
        else SET_VECTOR_ELT(argw, i, X);
      }
      if (!isNull(names)) SET_STRING_ELT(namesymbol, i, (i) ? STRING_ELT(names, i - 1) : mkChar(""));
      else SET_STRING_ELT(namesymbol, i, mkChar(""));
    }
    setAttrib(argw, R_NamesSymbol, namesymbol);
    PROTECT(argw);

    errorOccurred=0;
    PROTECT(lng = LCONS(Rhpc_docall, CONS(fun, CONS(argw, R_NilValue))));
    ret = R_tryEval(lng, R_GlobalEnv, &errorOccurred);
    
    if (errorOccurred) {
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
      ret = elist;
    }
    PROTECT(ret);
    PROTECT(tag_ret = allocVector(VECSXP, 2));
    SET_VECTOR_ELT(tag_ret, 0, tag);
    SET_VECTOR_ELT(tag_ret, 1, ret);

    PROTECT(l_out=R_NilValue);
    PROTECT(out=Rhpc_serialize_norealloc(tag_ret));

    {
      int cmdo[CMDLINESZ];
      R_xlen_t sz_out = xlength(out);
      R_xlen_t chunks_out, remainder_out;
      Rhpc_get_chunks(sz_out, &chunks_out, &remainder_out);
      
      int total_send_calls = (int)chunks_out + (remainder_out ? 1 : 0) + 1;
      MPI_Request *request = R_Calloc(total_send_calls, MPI_Request);
      MPI_Status *status = R_Calloc(total_send_calls, MPI_Status);
      int call_idx_out = 0;

      SET_CMD(cmdo, CMD_NAME_LAPPLY_LB, SUBCMD_NORMAL, chunks_out, remainder_out, usequote);
      _M(MPI_Isend(cmdo, (int)CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &request[call_idx_out++]));
      for (i = 0; i < chunks_out; i++) {
          _M(MPI_Isend(RAW(out) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &request[call_idx_out++]));
      }
      if (remainder_out != 0) {
          _M(MPI_Isend(RAW(out) + RHPC_SPLIT_SIZE * chunks_out, (int)remainder_out, MPI_CHAR, 0, TAGCAL(chunks_out), RHPC_Comm, &request[call_idx_out++]));
      }
      _M(MPI_Waitall(call_idx_out, request, status));
      R_Free(request); R_Free(status);
    }
    UNPROTECT(13);
  }
}
