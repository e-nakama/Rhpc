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
#include "RhpcWorker_Lapplyseq.h"

void Rhpc_worker_lapply_seq(int *cmd)
{
  int  errorOccurred=0;
  int  getcmd = 0, getsubcmd = 0, usequote = 0;
  R_xlen_t cnti = 0, modi = 0, leni, i;
  SEXP data, fun_arg, l_fun_arg, fun, arg, argq, XL, XL_l, resL, l_out, out;
  PROTECT_INDEX resL_ix;

  GET_CMD(cmd, &getcmd, &getsubcmd, &cnti, &modi, &usequote);
  leni = cnti * RHPC_SPLIT_SIZE + modi;
  PROTECT(data = allocVector(RAWSXP,leni));

  for(i = 0; i < cnti;i++) _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, RHPC_Comm));
  if( modi !=0 ) _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*cnti, (int)modi, MPI_CHAR, 0, RHPC_Comm));

  PROTECT(fun_arg=Rhpc_unserialize(data));
  PROTECT(l_fun_arg=R_NilValue);

  fun = VECTOR_ELT(fun_arg,0);
  if ( TYPEOF(fun) == STRSXP && xlength(fun)==1 ){
    SEXP find_fun = findFun(install(CHAR(STRING_ELT(fun,0))), R_GlobalEnv);
    if(find_fun != R_UnboundValue) fun = find_fun;
  } else if (TYPEOF(fun) == SYMSXP){
    SEXP find_fun = findFun(fun, R_GlobalEnv);
    if(find_fun != R_UnboundValue) fun = find_fun;
  }
  PROTECT(fun);
  PROTECT(arg = VECTOR_ELT(fun_arg,1));
  
  if(usequote) PROTECT(argq=Rhpc_enquote(arg));
  else PROTECT(argq=arg);
  
  {
    int cmdx[CMDLINESZ], getx = 0, getsubx = 0;
    R_xlen_t cntx = 0, modx = 0, lenx = 0;
    MPI_Status stat;
    SEXP datax;
    _M(MPI_Recv(cmdx, CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &stat));
    GET_CMD(cmdx, &getx, &getsubx, &cntx, &modx, &usequote);
    if( getsubx == SUBCMD_EXIT ){
      UNPROTECT(6);
      return;
    }
    lenx = RHPC_SPLIT_SIZE * cntx + modx;
    PROTECT(datax=allocVector(RAWSXP,lenx));
    int msgcnt = cntx+((modx)?1:0), calls=0;
    MPI_Request *requestx = R_Calloc(msgcnt, MPI_Request);
    MPI_Status *statusx = R_Calloc(msgcnt, MPI_Status);
    for(i=0;i<cntx;i++) _M(MPI_Irecv(RAW(datax)+RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &requestx[calls++]));
    if ( modx != 0 ) _M(MPI_Irecv(RAW(datax)+RHPC_SPLIT_SIZE*cntx, (int)modx, MPI_CHAR, 0, TAGCAL(cntx), RHPC_Comm, &requestx[calls++]));
    _M(MPI_Waitall(calls, requestx, statusx));
    R_Free(requestx); R_Free(statusx);
    PROTECT(XL_l=R_NilValue);
    PROTECT(XL =Rhpc_unserialize(datax));
  }

  R_xlen_t XLlen = xlength(XL), works;
  PROTECT(resL = allocVector(VECSXP,XLlen));
  PROTECT_WITH_INDEX(resL,&resL_ix);
  for(works=0; works<XLlen; works ++){
    SEXP X, argw, namesymbol, names, lng, ret;
    R_xlen_t argw_len;
    PROTECT(X = VECTOR_ELT(XL,works));
    PROTECT(names = getAttrib(argq, R_NamesSymbol));
    PROTECT(namesymbol = allocVector(STRSXP, xlength(argq)+1));
    argw=allocVector(VECSXP,xlength(argq)+1);
    argw_len = xlength(argw);
    for (i=0;i<argw_len;i++){
      if(i) SET_VECTOR_ELT(argw,i,VECTOR_ELT(argq,i-1));
      else {
        if (usequote) SET_VECTOR_ELT(argw,i,LCONS(install("quote"),CONS(X,R_NilValue)));
        else SET_VECTOR_ELT(argw,i,X);
      }
      if(!isNull(names)) SET_STRING_ELT(namesymbol,i,(i)?STRING_ELT(names, i-1):mkChar(""));
      else SET_STRING_ELT(namesymbol,i,mkChar(""));
    }
    setAttrib(argw, R_NamesSymbol, namesymbol);
    PROTECT(argw);

    errorOccurred=0;
    if(usequote) PROTECT(lng = LCONS(Rhpc_docall, CONS(fun,CONS(argw, R_NilValue))));
    else PROTECT(lng = LCONS(install("do.call"), CONS(fun, CONS(argw, R_NilValue))));
    PROTECT(ret=R_tryEval(lng, R_GlobalEnv, &errorOccurred));

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
    SET_VECTOR_ELT(resL,works,ret);
    REPROTECT(resL,resL_ix);
    UNPROTECT(6);
  }
 
  PROTECT(l_out=R_NilValue);
  PROTECT(out=Rhpc_serialize_norealloc(resL));

  {
    int cmdo[CMDLINESZ];
    R_xlen_t sz = xlength(out);
    R_xlen_t cnto = sz / RHPC_SPLIT_SIZE;
    R_xlen_t modo = sz % RHPC_SPLIT_SIZE;
    int reqcnt = cnto+((modo)?1:0);
    MPI_Request *request = R_Calloc(reqcnt+1, MPI_Request);
    MPI_Status *status = R_Calloc(reqcnt+1, MPI_Status);
    int calls=0;

    SET_CMD(cmdo, CMD_NAME_LAPPLY_SEQ, SUBCMD_NORMAL, cnto, modo, usequote );
    _M(MPI_Isend(cmdo, (int)CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &request[calls++]));
    for( i = 0 ; i< cnto ; i++) _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, TAGCAL(i), RHPC_Comm, &request[calls++]));
    if ( modo != 0 ) _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*cnto, (int)modo, MPI_CHAR, 0, TAGCAL(cnto), RHPC_Comm, &request[calls++]));
    _M(MPI_Waitall(calls, request, status));
    R_Free(request); R_Free(status);
  }
  UNPROTECT(13);

  {
    int cmdx[CMDLINESZ], getx = 0, getsubx = 0;
    R_xlen_t cntx = 0, modx = 0;
    MPI_Status stat;
    _M(MPI_Recv(cmdx, CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &stat));
    GET_CMD(cmdx, &getx, &getsubx, &cntx, &modx, &usequote);
    if( getsubx == SUBCMD_EXIT ) return;
  }
}
