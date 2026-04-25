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
#include <sys/time.h>
SEXP Rhpc_mpi_worker_call(SEXP cl, SEXP args, SEXP actioncode, SEXP usequote)
{
  int action=INTEGER(actioncode)[0];

  R_xlen_t i,j;

  MPI_Comm comm;
  int procs;

  SEXP out, l_out=R_NilValue;

  R_xlen_t szi;
  R_xlen_t cnti;
  R_xlen_t modi;
  int cmd[CMDLINESZ];
  int dummy_cmd[CMDLINESZ];

  int *cmds;

  SEXP inlist;
  R_xlen_t *szs;
  R_xlen_t *cnts;
  R_xlen_t *mods;
  int       reqcnt;

  SEXP inlista;
  PROTECT_INDEX ix0;

  MPI_Request *request;
  MPI_Status  *status;

  int calls;

  SEXP outlist;

  if(TYPEOF(cl)!=EXTPTRSXP){
    error("it's not MPI_Comm external pointer\n");
  }
  comm = SXP2COMM(cl);

  _M(MPI_Comm_size(comm, &procs));

  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(!initialize){
    warning("Rhpc not initialized.");
    return(R_NilValue);
  }

  push_policy();

  /* serialize */
  GETTIME(ts);
  PROTECT(l_out);
  PROTECT(out=Rhpc_serialize_norealloc(args));
  GETTIME(te);
  TMPRINT("serialize:%f\n");

  /* cmd send */
  GETTIME(ts);
  szi = xlength(out);
  cnti = szi/RHPC_SPLIT_SIZE;
  modi = szi%RHPC_SPLIT_SIZE;
  switch (action){
  case 0:
    SET_CMD(cmd, CMD_NAME_WORKERCALL_NORET,  SUBCMD_NORMAL, cnti, modi, INTEGER(usequote)[0]);
    break;
  case 2:
    SET_CMD(cmd, CMD_NAME_WORKERCALL_EXPORT, SUBCMD_NORMAL, cnti, modi, INTEGER(usequote)[0]);
    break;
  default:
    SET_CMD(cmd, CMD_NAME_WORKERCALL_RET,    SUBCMD_NORMAL, cnti, modi, INTEGER(usequote)[0]);
    break;
  }
  _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, comm));
  GETTIME(te);
  TMPRINT("send cmd:%f\n");

  /* send data */
  GETTIME(ts);
  for(i = 0; i < cnti;i++){
    _M(MPI_Bcast(RAW(out)+ RHPC_SPLIT_SIZE*i,    (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, comm));
  }
  if( modi !=0 ){
    _M(MPI_Bcast(RAW(out)+ RHPC_SPLIT_SIZE*cnti, (int)modi,            MPI_CHAR, 0, comm));
  }
  GETTIME(te);
  TMPRINT("send data:%f\n");

  if(action==0){/* Rhpc_worker_shy and Rhpc_worker_noback */
      SEXP nillist;
      PROTECT(nillist=allocVector(VECSXP,procs-1));
      for(i=1; i<procs; i++){
	SET_VECTOR_ELT(nillist, i-1, R_NilValue);
      }
      UNPROTECT(3);

      pop_policy();

      return(nillist);
  }

  /* recive alloc; */
  GETTIME(ts);
  cmds = Calloc(procs * CMDLINESZ, int);
  memset(cmds,0,procs * CMDLINESZ * sizeof(int));
  _M(MPI_Gather(dummy_cmd, CMDLINESZ, MPI_INT, cmds, CMDLINESZ, MPI_INT, 0, comm));

  szs  = Calloc(procs,R_xlen_t);
  cnts = Calloc(procs,R_xlen_t);
  mods = Calloc(procs,R_xlen_t);
  reqcnt=0;
  memset((void*)szs ,0,procs*sizeof(R_xlen_t));
  memset((void*)cnts,0,procs*sizeof(R_xlen_t));
  memset((void*)mods,0,procs*sizeof(R_xlen_t));


  PROTECT(inlist=allocVector(VECSXP,procs-1));
  inlista=R_NilValue;
  PROTECT_WITH_INDEX(inlista,&ix0);

  for(i=1; i<procs; i++){
    int dummy_main;
    int dummy_sub;
    int dummy_usequote;
    GET_CMD(cmds+CMDLINESZ*i, &dummy_main, &dummy_sub, &cnts[i], &mods[i], &dummy_usequote); 
    szs[i] = cnts[i] * RHPC_SPLIT_SIZE + mods[i];
    reqcnt += cnts[i] + ((mods[i])?1:0);
    REPROTECT(inlista = allocVector(RAWSXP,szs[i]), ix0);
    SET_VECTOR_ELT(inlist, i-1,inlista);
  }

  request = Calloc(reqcnt,MPI_Request);
  status  = Calloc(reqcnt,MPI_Status);
  GETTIME(te);
  TMPRINT("recive alloc:%f\n");

  /* recive */
  GETTIME(ts);
  for(calls=0,i=1; i<procs; i++){
    for(j=0;j<cnts[i];j++){
      _M(MPI_Irecv(RAW(VECTOR_ELT(inlist,i-1))+RHPC_SPLIT_SIZE*j,
		   (int)RHPC_SPLIT_SIZE,
		   MPI_CHAR, i, TAGCAL(j),
		   comm, &request[calls]));
      calls++;
    }
    if ( mods[i] != 0 ){
      _M(MPI_Irecv(RAW(VECTOR_ELT(inlist,i-1))+RHPC_SPLIT_SIZE*cnts[i],
		   (int)mods[i],
		   MPI_CHAR, i, TAGCAL(cnts[i]),
		   comm, &request[calls]));
      calls++;
    }
  }
  _M(MPI_Waitall(calls, request, status));

  Free(request);
  Free(status);
  GETTIME(te);
  TMPRINT("recive:%f\n");

  /* unserialize */
  {
    PROTECT_INDEX ix1;
    PROTECT_INDEX ix2;
    SEXP un, l_un;
    GETTIME(ts);
    PROTECT(outlist=allocVector(VECSXP,procs-1));
    PROTECT_WITH_INDEX(l_un=R_NilValue,&ix1);
    PROTECT_WITH_INDEX(  un=R_NilValue,&ix2);
    for(i=1; i<procs; i++){
      REPROTECT(un = Rhpc_unserialize(VECTOR_ELT(inlist,i-1)), ix2);
      SET_VECTOR_ELT(outlist, i-1, un);
    }
    GETTIME(te);
    TMPRINT("unserialize:%f\n");
  }
  UNPROTECT(7);

  Free(cmds);
  Free(szs);
  Free(cnts);
  Free(mods);

  pop_policy();

  return(_CHK(outlist));
}
