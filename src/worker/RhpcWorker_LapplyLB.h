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

static void Rhpc_worker_lapply_LB(int *cmd)
{
  int  errorOccurred=0;
  int  getcmd = 0;
  int  getsubcmd = 0;
  R_xlen_t cnti = 0;
  R_xlen_t modi = 0;
  int  usequote=0;

  /* data recive alloc */
  R_xlen_t leni;
  SEXP data;
  R_xlen_t  i;
  /* unserialize */
  SEXP fun_arg, l_fun_arg;
  /* find function */
  SEXP fun;
  SEXP arg;
  /* quote */
  SEXP argq;


  GET_CMD(cmd, &getcmd, &getsubcmd, &cnti, &modi, &usequote);

  /* data recive alloc */
  leni = cnti * RHPC_SPLIT_SIZE + modi;
  PROTECT(data = allocVector(RAWSXP,leni));

  /* data recive */
  for(i = 0; i < cnti;i++){
    _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*i,    (int)RHPC_SPLIT_SIZE,
		 MPI_CHAR, 0, RHPC_Comm));
   }
  if( modi !=0 ){
    _M(MPI_Bcast(RAW(data)+ RHPC_SPLIT_SIZE*cnti, (int)modi,
		 MPI_CHAR, 0, RHPC_Comm));
   }

  /* unserialize */
  PROTECT(fun_arg=Rhpc_unserialize(data));
  PROTECT(l_fun_arg=R_NilValue);

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
  

  /* quote */
  if(usequote)
    PROTECT(argq=Rhpc_enquote(arg));
  else
    PROTECT(argq=arg);

  for(;1;){
    /* get X argument */
    int cmdx[CMDLINESZ];
    int  getx = 0;
    int  getsubx = 0;
    R_xlen_t cntx = 0;
    R_xlen_t modx = 0;
    R_xlen_t lenx = 0;
    MPI_Status stat;
    SEXP datax;
    R_xlen_t msgcnt;
    int calls;
    MPI_Request *requestx;
    MPI_Status  *statusx;
    SEXP tag_X;
    SEXP tag_X_l;
    SEXP tag;
    SEXP X;
    SEXP argw;
    SEXP namesymbol;
    SEXP names;
    R_xlen_t argw_len;
    SEXP lng,ret;
    SEXP tag_ret;
    SEXP l_out,out;

    _M(MPI_Recv(cmdx, CMDLINESZ, MPI_INT, 0, RHPC_CTRL_TAG, RHPC_Comm, &stat));
    /*
      mydump((void*)cmdx,CMDLINESZ*sizeof(int));
    */
    GET_CMD(cmdx, &getx, &getsubx, &cntx, &modx, &usequote);
    if( getsubx == SUBCMD_EXIT ){
      UNPROTECT(6);
      return;
    }
    lenx = RHPC_SPLIT_SIZE * cntx + modx;
    DPRINT("lenx=%ld\n",lenx);


    PROTECT(datax=allocVector(RAWSXP,lenx));
    
    msgcnt = cntx+((modx)?1:0);
    calls=0;
    requestx=Calloc(msgcnt, MPI_Request);
    statusx =Calloc(msgcnt, MPI_Status);
    DPRINT("success datax(%ld)\n",xlength(datax));
      
    for(i=0;i<cntx;i++){
      _M(MPI_Irecv(RAW(datax)+RHPC_SPLIT_SIZE*i, 
		   (int)RHPC_SPLIT_SIZE, MPI_CHAR,
		   0, TAGCAL(i),
		   RHPC_Comm, &requestx[calls]));
      calls++;
    }
    if ( modx != 0 ){
      _M(MPI_Irecv(RAW(datax)+RHPC_SPLIT_SIZE*cntx,
		   (int)modx,            MPI_CHAR,
		   0, TAGCAL(cntx),
		   RHPC_Comm, &requestx[calls]));
      DPRINT("RECV end\n");
      calls++;
    }
    _M(MPI_Waitall(calls, requestx, statusx));
    Free(requestx);
    Free(statusx);
    DPRINT("success recv x\n");

    PROTECT(tag_X_l=R_NilValue);
    PROTECT(tag_X  = Rhpc_unserialize(datax));

    PROTECT(tag = VECTOR_ELT(tag_X,0));
    PROTECT(X   = VECTOR_ELT(tag_X,1));
    DPRINT("success unserialize x\n");

    /* make args */
    PROTECT(names = getAttrib(argq, R_NamesSymbol));
    PROTECT(namesymbol = allocVector(STRSXP, xlength(argq)+1));
    argw=allocVector(VECSXP,xlength(argq)+1);
    argw_len = xlength(argw);
    for (i=0;i<argw_len;i++){
      if(i)
	SET_VECTOR_ELT(argw,i,VECTOR_ELT(argq,i-1));
      else
	if(usequote)
	  SET_VECTOR_ELT(argw,i,LCONS(install("quote"),CONS(X,R_NilValue)));
	else
	  SET_VECTOR_ELT(argw,i,X);
      if(!isNull(names)){
	SET_STRING_ELT(namesymbol,i,(i)?STRING_ELT(names, i-1):mkChar(""));
      }else{
	SET_STRING_ELT(namesymbol,i,mkChar(""));
      }
    }
    setAttrib(argw, R_NamesSymbol, namesymbol);
    PROTECT(argw);
    /*
    //STR(arg);
    //STR(argq);
    //STR(argw);
    */

    /* eval */
    errorOccurred=0;
    if(usequote)
      PROTECT(lng = LCONS(Rhpc_docall, CONS(fun,CONS(argw, R_NilValue))));
    else
      PROTECT(lng = LCONS(install("do.call"), CONS(fun, CONS(argw, R_NilValue))));
    ret=R_tryEval(lng, R_GlobalEnv, &errorOccurred);
    DPRINT("errorOccurred=%d\n",errorOccurred);
    
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
    DPRINT("success eval x\n");

    PROTECT(tag_ret=allocVector(VECSXP,2));
    SET_VECTOR_ELT(tag_ret,0,tag);
    SET_VECTOR_ELT(tag_ret,1,ret);
    DPRINT("success add tag x\n");

    /* serialize */
    PROTECT(l_out=R_NilValue);
    PROTECT(out=Rhpc_serialize_norealloc(tag_ret));
    DPRINT("success  serialize outlength=%ld\n",xlength(out));


    /* send */
    {
      int cmdo[CMDLINESZ];
      R_xlen_t sz;
      R_xlen_t cnto;
      R_xlen_t modo;
      int reqcnt;
      MPI_Request *request;
      MPI_Status  *status;
      int calls;

      sz =  xlength(out);
      cnto = (int)(sz / RHPC_SPLIT_SIZE);
      modo = (int)(sz % RHPC_SPLIT_SIZE);
      reqcnt = cnto+((modo)?1:0);

      request = Calloc(reqcnt+1,  MPI_Request);
      status  = Calloc(reqcnt+1,  MPI_Status);

      SET_CMD(cmdo, CMD_NAME_LAPPLY_LB, SUBCMD_NORMAL, cnto, modo, usequote );

      calls=0;
      _M(MPI_Isend(cmdo,   (int)CMDLINESZ,
		   MPI_INT, 0, RHPC_CTRL_TAG,
		   RHPC_Comm, &request[calls]));
      DPRINT("send ctrl\n");
      calls++;
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
      DPRINT("send data wait start\n");
      _M(MPI_Waitall(calls, request, status));
      Free(request);
      Free(status);
      DPRINT("send data wait end\n");
    }
    UNPROTECT(13);
  }
  return;
}

