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


static void Rhpc_worker_call(int *cmd, int action)
{
  int  errorOccurred=0;
  int  getcmd = 0;
  int  getsubcmd = 0;
  R_xlen_t cnti = 0;
  R_xlen_t modi = 0;
  int  usequote = 0;
  /* data recive alloc */
  R_xlen_t leni;
  SEXP data;
  R_xlen_t i;
  /* unserialize */
  SEXP fun_arg, l_fun_arg;
  /* find function */
  SEXP fun;
  SEXP arg;
  SEXP argnm;
  /* quote */
  SEXP argq;
  /* elav */
  SEXP lng,ret;
  /* serialize */
  SEXP l_out,out;

  GET_CMD(cmd, &getcmd, &getsubcmd, &cnti, &modi, &usequote);

  /* data recive alloc */
  leni = cnti * RHPC_SPLIT_SIZE + modi;
  PROTECT(data = allocVector(RAWSXP,leni));

  /* data recive */
  for(i = 0; i < cnti;i++){
    _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*i,    (int)RHPC_SPLIT_SIZE,  MPI_CHAR, 0, RHPC_Comm));
   }
  if( modi !=0 ){
    _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*cnti, (int)modi,             MPI_CHAR, 0, RHPC_Comm));
   }

  /* unserialize */
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
    /* find function */
    fun = VECTOR_ELT(fun_arg,0);
    if ( TYPEOF(fun) == STRSXP && xlength(fun)==1 ){
      SEXP find_fun = findVar(install(CHAR(STRING_ELT(fun,0))), R_GlobalEnv);
      if(find_fun !=  R_UnboundValue) fun = find_fun;
    } else if (TYPEOF(fun) == SYMSXP){
      SEXP find_fun = findVar(fun, R_GlobalEnv);
      if(find_fun !=  R_UnboundValue) fun = find_fun;
    }
    PROTECT(fun);
    PROTECT(arg = VECTOR_ELT(fun_arg,1));
    PROTECT(argnm=R_NilValue);

  }
  /* quote */
  if (usequote)
    PROTECT(argq=Rhpc_enquote(arg));
  else
    PROTECT(argq=arg);

  /* eval */
  errorOccurred=0;
  
  if (usequote)
    PROTECT(lng = LCONS(Rhpc_docall, CONS(fun,CONS(argq, R_NilValue))));
  else
    PROTECT(lng = LCONS(install("do.call"), CONS(fun, CONS(argq, R_NilValue))));

  ret=R_tryEval(lng, R_GlobalEnv, &errorOccurred);
    
  if(action == 0){
    UNPROTECT(8);
    return;
  }
  
  if(errorOccurred){
    SEXP eclass;
    SEXP elist;
    SEXP elist_label;
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

  
  /* serialize */
  PROTECT(l_out=R_NilValue);
  PROTECT(out=Rhpc_serialize_norealloc(ret));

  /* send */
  {
    int cmd[CMDLINESZ];
    int dummy_cmd[CMDLINESZ];



    R_xlen_t sz =  xlength(out);
    R_xlen_t cnto = (int)(sz / RHPC_SPLIT_SIZE);
    R_xlen_t modo = (int)(sz % RHPC_SPLIT_SIZE);
    int reqcnt = cnto+((modo)?1:0);

    MPI_Request *request=Calloc(reqcnt,MPI_Request);
    MPI_Status  *status =Calloc(reqcnt,MPI_Status);
    int calls;

    SET_CMD(cmd, getcmd, SUBCMD_NORMAL, cnto, modo, usequote );

    _M(MPI_Gather(cmd, CMDLINESZ, MPI_INT, dummy_cmd, CMDLINESZ, MPI_INT, 0, RHPC_Comm));

    calls=0;
    for( i = 0 ; i< cnto ; i++){
      _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*i,   (int)RHPC_SPLIT_SIZE,
		   MPI_CHAR, 0, TAGCAL(i),
		   RHPC_Comm, &request[calls]));
      calls++;
    }
    if ( modo != 0 ){
      _M(MPI_Isend(RAW(out)+RHPC_SPLIT_SIZE*cnto, (int)modo,
		   MPI_CHAR, 0, TAGCAL(cnto),
		   RHPC_Comm, &request[calls]));
      calls++;
    }
    _M(MPI_Waitall(calls, request, status));

    Free(request);
    Free(status);
  }
  UNPROTECT(11);

  return;
}

