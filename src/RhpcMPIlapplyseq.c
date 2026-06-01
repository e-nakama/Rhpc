/*
    Rhpc : R HPC environment
    Copyright (C) 2012-2026 Ei-ji Nakama and Junji NAKANO

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef WIN32
#include "common/config.h"
#endif

#include <mpi.h>
#include <R.h>
#include <Rinternals.h>
#include "common/Rhpc.h"
#include "RhpcMPIlapplyseq.h"

static R_xlen_t Rhpc_mpi_lapply_seq_worker_calls(R_xlen_t *calls,int procs)
{
  int i;
  R_xlen_t cnt=0;
  for (i=1;i<procs;i++) cnt+=calls[i];
  return(cnt);
}

static void Rhpc_mpi_lapply_seq_exit(int procs, MPI_Comm comm)
{
  R_xlen_t i;
  int cmde[CMDLINESZ];
  MPI_Request *request=R_Calloc((procs-1),MPI_Request);
  MPI_Status  *status =R_Calloc((procs-1),MPI_Status);
  SET_CMD(cmde, CMD_NAME_LAPPLY_SEQ , SUBCMD_EXIT, 0, 0, 0 );
  for (i=1; i < procs ; i++) _M(MPI_Isend(cmde, (int)CMDLINESZ, MPI_INT, i, RHPC_CTRL_TAG, comm, &request[i-1]));
  _M(MPI_Waitall(procs-1, request, status));
  R_Free(request);
  R_Free(status);
}

static void Rhpc_mpi_lapply_seq_send(R_xlen_t *workers, int procs, SEXP X, SEXP usequote, MPI_Comm comm)
{
  R_xlen_t i,j;
  SEXP Xsel, sendlist, XL;
  PROTECT_INDEX ix2;
  R_xlen_t sendcalls;
  MPI_Request *request;
  MPI_Status  *status;

  PROTECT_WITH_INDEX(Xsel = R_NilValue, &ix2);
  PROTECT(sendlist=allocVector(VECSXP,procs-1));
  for (i=1; i < procs ; i++) SET_VECTOR_ELT(sendlist,i-1, R_NilValue);
  PROTECT(XL = Rhpc_splitList(X,ScalarInteger(procs-1)));
  
  for (i=1; i < procs ; i++){
    workers[i]=xlength(VECTOR_ELT(XL,i-1));
    REPROTECT(Xsel= Rhpc_serialize_norealloc(VECTOR_ELT(XL,i-1)), ix2);
    SET_VECTOR_ELT(sendlist,i-1, Xsel);
  }
  
  sendcalls=0;
  for (i=1; i < procs ; i++){
    R_xlen_t lens = xlength(VECTOR_ELT(sendlist,i-1));
    R_xlen_t cnts = lens / RHPC_SPLIT_SIZE;
    R_xlen_t mods = lens % RHPC_SPLIT_SIZE;
    sendcalls+= 1 + cnts + ((mods)?1:0);
  }
  
  request=R_Calloc(sendcalls,MPI_Request);
  status =R_Calloc(sendcalls,MPI_Status);
  
  if (sendcalls){
    sendcalls=0;
    for (i=1; i < procs ; i++){
      int cmds[CMDLINESZ];
      R_xlen_t lens = xlength(VECTOR_ELT(sendlist,i-1));
      R_xlen_t cnts = lens / RHPC_SPLIT_SIZE;
      R_xlen_t mods = lens % RHPC_SPLIT_SIZE;
      SET_CMD(cmds, CMD_NAME_LAPPLY_SEQ , SUBCMD_NORMAL, cnts, mods, INTEGER(usequote)[0] );
      _M(MPI_Isend(cmds, (int)CMDLINESZ, MPI_INT, i, RHPC_CTRL_TAG, comm, &request[sendcalls++]));
      for( j = 0 ; j< cnts ; j++) _M(MPI_Isend(RAW(VECTOR_ELT(sendlist,i-1))+RHPC_SPLIT_SIZE*j, (int)RHPC_SPLIT_SIZE, MPI_CHAR, (int)i, TAGCAL(j), comm, &request[sendcalls++]));
      if ( mods != 0 ) _M(MPI_Isend(RAW(VECTOR_ELT(sendlist,i-1))+RHPC_SPLIT_SIZE*cnts, (int)mods, MPI_CHAR, (int)i, TAGCAL(cnts), comm, &request[sendcalls++]));
    }
    _M(MPI_Waitall(sendcalls, request, status));
    R_Free(request);
    R_Free(status);
  }
  UNPROTECT(3);
}

SEXP Rhpc_mpi_lapply_seq(SEXP cl, SEXP X, SEXP args, SEXP usequote)
{
  R_xlen_t i;
  MPI_Comm comm;
  int procs;
  SEXP out, l_out=R_NilValue;
  R_xlen_t szi, cnti, modi;
  int dummy_usequote, cmd[CMDLINESZ];
  R_xlen_t xlen = xlength(X), *workers, *workersix;
  SEXP outlist_l, outlist, names = getAttrib(X, R_NamesSymbol);
  PROTECT_INDEX outlist_l_ix, outlist_ix;
  SEXP indata, uns, uns_l;
  PROTECT_INDEX indata_ix, uns_l_ix, uns_ix;

  if(TYPEOF(cl)!=EXTPTRSXP) error("%s", "it's not MPI_Comm external pointer\n");
  comm = SXP2COMM(cl);
  _M(MPI_Comm_size(comm, &procs));

  if(finalize){ warning("Rhpc were already finalized."); return(R_NilValue); }
  if(!initialize){ warning("Rhpc not initialized."); return(R_NilValue); }

  push_policy();
  PROTECT(names); PROTECT(l_out);
  PROTECT(out=Rhpc_serialize_norealloc(args));

  szi = xlength(out);
  cnti = szi/RHPC_SPLIT_SIZE;
  modi = szi%RHPC_SPLIT_SIZE;
  SET_CMD(cmd, CMD_NAME_LAPPLY_SEQ, SUBCMD_NORMAL,cnti, modi, INTEGER(usequote)[0]);
  _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, comm));

  for(i = 0; i < cnti;i++) _M(MPI_Bcast(RAW(out)+ RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, comm));
  if( modi !=0 ) _M(MPI_Bcast(RAW(out)+ RHPC_SPLIT_SIZE*cnti, (int)modi, MPI_CHAR, 0, comm));

  workers=R_Calloc(procs,R_xlen_t);
  workersix=R_Calloc(procs,R_xlen_t);
  memset((void*)workers,0,sizeof(R_xlen_t)*procs);
  for(i=0;i<procs;i++)  workersix[i]=i-1;

  PROTECT_WITH_INDEX(outlist_l=R_NilValue, &outlist_l_ix);
  PROTECT_WITH_INDEX(outlist=allocVector(VECSXP,xlen), &outlist_ix);
  PROTECT_WITH_INDEX(indata=R_NilValue, &indata_ix);
  PROTECT_WITH_INDEX(uns_l=R_NilValue, &uns_l_ix);
  PROTECT_WITH_INDEX(uns=R_NilValue, &uns_ix);

  if(!isNull(names)) setAttrib(outlist, R_NamesSymbol, names);
  Rhpc_mpi_lapply_seq_send(workers, procs, X, usequote, comm);

  while( Rhpc_mpi_lapply_seq_worker_calls(workers,procs)){
    MPI_Status stat;
    _M(MPI_Probe(MPI_ANY_SOURCE, RHPC_CTRL_TAG, comm, &stat));
    if(stat.MPI_SOURCE != 0){
	int cmdr[CMDLINESZ], wkr = stat.MPI_SOURCE;
	int cmdmainr = 0, cmdsubr = 0;
	R_xlen_t cntr = 0, modr = 0, lenr = 0;
	int msgcnt, calls;
	_M(MPI_Recv(cmdr, CMDLINESZ, MPI_INT, wkr, RHPC_CTRL_TAG, comm, &stat));
	GET_CMD(cmdr, &cmdmainr, &cmdsubr, &cntr, &modr, &dummy_usequote);
	lenr = RHPC_SPLIT_SIZE * cntr + modr;
	REPROTECT(indata=allocVector(RAWSXP,lenr),indata_ix);
	
	msgcnt = cntr+((modr)?1:0);
	calls=0;
	{
	  MPI_Request *request = R_Calloc(msgcnt, MPI_Request);
	  MPI_Status  *status  = R_Calloc(msgcnt, MPI_Status);
	  for(i=0;i<cntr;i++){
	    _M(MPI_Irecv(RAW(indata)+RHPC_SPLIT_SIZE*i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, wkr, TAGCAL(i), comm, &request[calls++]));
	  }
	  if ( modr != 0 ) _M(MPI_Irecv(RAW(indata)+RHPC_SPLIT_SIZE*cntr, (int)modr, MPI_CHAR, wkr, TAGCAL(cntr), comm, &request[calls++]));
	  _M(MPI_Waitall(calls, request, status));
	  R_Free(status); R_Free(request);
	}
	
	REPROTECT(uns = Rhpc_unserialize(indata),uns_ix);

	for(int wkrix=0 ; wkrix<xlength(uns); wkrix++){
	  SET_VECTOR_ELT(outlist, (wkr-1)+wkrix*(procs-1), VECTOR_ELT(uns,wkrix) );
	}
	workers[wkr]-=xlength(uns);
	workersix[wkr]+=procs-1;
	DPRINT("finish rank=%d ix=%ld\n", wkr, workersix[wkr] );
    }
  }
  Rhpc_mpi_lapply_seq_exit(procs, comm);
  UNPROTECT(8);
  R_Free(workers);
  R_Free(workersix);

  pop_policy();
  return (_CHK(outlist));
}