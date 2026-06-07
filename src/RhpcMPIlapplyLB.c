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

#include "common/Rhpc.h"
#include <mpi.h>
#include <R.h>
#include <Rinternals.h>
#include "RhpcMPIlapplyLB.h"

/* Worker states for Load Balancing */
typedef enum {
    WORKER_IDLE = 0,
    WORKER_BUSY = 1
} WorkerState;

static void Rhpc_mpi_lapply_LB_exit(int num_procs, MPI_Comm comm)
{
    int cmde[CMDLINESZ];
    MPI_Request *request = R_Calloc((num_procs - 1), MPI_Request);
    MPI_Status *status = R_Calloc((num_procs - 1), MPI_Status);

    SET_CMD(cmde, CMD_NAME_LAPPLY_LB, SUBCMD_EXIT, 0, 0, 0);
    for (int i = 1; i < num_procs; i++) {
        DPRINT("worker exit rank=%d\n", i);
        _M(MPI_Isend(cmde, (int)CMDLINESZ, MPI_INT, i, RHPC_CTRL_TAG, comm, &request[i - 1]));
    }
    _M(MPI_Waitall(num_procs - 1, request, status));
    R_Free(request);
    R_Free(status);
}

static void Rhpc_mpi_lapply_LB_send(WorkerState *worker_status, int num_procs,
                                    R_xlen_t *next_item, SEXP X,
                                    SEXP usequote, MPI_Comm comm,
                                    int *active_workers)
{
    R_xlen_t xlen = xlength(X);
    SEXP sendlist, tag, Xsel;
    PROTECT_INDEX ix2, ix3;

    PROTECT(sendlist = allocVector(VECSXP, num_procs - 1));
    for (int i = 0; i < num_procs - 1; i++) SET_VECTOR_ELT(sendlist, i, R_NilValue);
    PROTECT_WITH_INDEX(tag = R_NilValue, &ix3);
    PROTECT_WITH_INDEX(Xsel = R_NilValue, &ix2);

    R_xlen_t total_send_calls = 0;

    /* Prepare work for idle workers */
    for (int i = 1; i < num_procs && *next_item < xlen; i++) {
        if (worker_status[i] == WORKER_IDLE) {
            REPROTECT(tag = allocVector(VECSXP, 2), ix3);
            SET_VECTOR_ELT(tag, 0, ScalarReal((double)(*next_item)));
            SET_VECTOR_ELT(tag, 1, VECTOR_ELT(X, *next_item));

            REPROTECT(Xsel = Rhpc_serialize_norealloc(tag), ix2);
            SET_VECTOR_ELT(sendlist, i - 1, Xsel);

            R_xlen_t lens = xlength(Xsel);
            R_xlen_t cnts, mods;
            Rhpc_get_chunks(lens, &cnts, &mods);
            total_send_calls += 1 + cnts + (mods ? 1 : 0);

            worker_status[i] = WORKER_BUSY;
            (*active_workers)++;
            (*next_item)++;
        }
    }

    if (total_send_calls > 0) {
        MPI_Request *request = R_Calloc(total_send_calls, MPI_Request);
        MPI_Status *status = R_Calloc(total_send_calls, MPI_Status);
        R_xlen_t call_idx = 0;

        for (int i = 1; i < num_procs; i++) {
            SEXP current_payload = VECTOR_ELT(sendlist, i - 1);
            if (current_payload != R_NilValue) {
                int cmds[CMDLINESZ];
                R_xlen_t lens = xlength(current_payload);
                R_xlen_t cnts, mods;
                Rhpc_get_chunks(lens, &cnts, &mods);

                SET_CMD(cmds, CMD_NAME_LAPPLY_LB, SUBCMD_NORMAL, cnts, mods, INTEGER(usequote)[0]);
                _M(MPI_Isend(cmds, (int)CMDLINESZ, MPI_INT, i, RHPC_CTRL_TAG, comm, &request[call_idx++]));

                for (R_xlen_t j = 0; j < cnts; j++) {
                    _M(MPI_Isend(RAW(current_payload) + RHPC_SPLIT_SIZE * j, (int)RHPC_SPLIT_SIZE,
                                 MPI_CHAR, i, TAGCAL(j), comm, &request[call_idx++]));
                }
                if (mods != 0) {
                    _M(MPI_Isend(RAW(current_payload) + RHPC_SPLIT_SIZE * cnts, (int)mods,
                                 MPI_CHAR, i, TAGCAL(cnts), comm, &request[call_idx++]));
                }
            }
        }
        _M(MPI_Waitall((int)total_send_calls, request, status));
        R_Free(request);
        R_Free(status);
    }
    UNPROTECT(3);
}

SEXP Rhpc_mpi_lapply_LB(SEXP cl, SEXP X, SEXP args, SEXP usequote)
{
  R_xlen_t i;
  MPI_Comm comm;
  int num_procs;
  SEXP out, l_out=R_NilValue;
  R_xlen_t szi, cnti, modi;
  int dummy_quote;
  int cmd[CMDLINESZ];
  R_xlen_t xlen, next_item = 0;
  WorkerState *worker_status;
  int active_workers = 0;
  SEXP outlist_l, outlist, ans, names = getAttrib(X, R_NamesSymbol);
  PROTECT_INDEX outlist_l_ix, outlist_ix, ans_ix;
  SEXP indata, uns, uns_l;
  PROTECT_INDEX indata_ix, uns_l_ix, uns_ix;

  if(TYPEOF(cl)!=EXTPTRSXP){
    error("%s", "it's not MPI_Comm external pointer\n");
  }
  comm = SXP2COMM(cl);
  _M(MPI_Comm_size(comm, &num_procs));

  if(finalize){
    warning("Rhpc were already finalized.");
    return(R_NilValue);
  }
  if(!initialize){
    warning("Rhpc not initialized.");
    return(R_NilValue);
  }

  push_policy();
  PROTECT(names);
  PROTECT(l_out);
  PROTECT(out=Rhpc_serialize_norealloc(args));

  szi = xlength(out);
  Rhpc_get_chunks(szi, &cnti, &modi);
  SET_CMD(cmd, CMD_NAME_LAPPLY_LB, SUBCMD_NORMAL, cnti, modi, INTEGER(usequote)[0]);
  _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, comm));

  for (i = 0; i < cnti; i++) _M(MPI_Bcast(RAW(out) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, comm));
  if (modi != 0) _M(MPI_Bcast(RAW(out) + RHPC_SPLIT_SIZE * cnti, (int)modi, MPI_CHAR, 0, comm));

  xlen = xlength(X);
  worker_status = R_Calloc(num_procs, WorkerState);
  memset(worker_status, 0, sizeof(WorkerState) * num_procs);

  PROTECT_WITH_INDEX(outlist_l=R_NilValue,               &outlist_l_ix);
  PROTECT_WITH_INDEX(outlist  =allocVector(VECSXP,xlen), &outlist_ix);
  PROTECT_WITH_INDEX(ans      =R_NilValue,               &ans_ix);
  PROTECT_WITH_INDEX(indata   =R_NilValue,               &indata_ix);
  PROTECT_WITH_INDEX(uns_l    =R_NilValue,               &uns_l_ix);
  PROTECT_WITH_INDEX(uns      =R_NilValue,               &uns_ix);

  if (!isNull(names)) setAttrib(outlist, R_NamesSymbol, names);

  /* Main Loop: Distribute work and collect results */
  while (next_item < xlen || active_workers > 0) {
    MPI_Status stat;
    Rhpc_mpi_lapply_LB_send(worker_status, num_procs, &next_item, X, usequote, comm, &active_workers);

    if (active_workers > 0) {
      _M(MPI_Probe(MPI_ANY_SOURCE, RHPC_CTRL_TAG, comm, &stat));
      if(stat.MPI_SOURCE != 0){
	int cmdr[CMDLINESZ];
	int wkr = stat.MPI_SOURCE;
	int cmdmainr = 0, cmdsubr = 0;
	R_xlen_t cntr = 0, modr = 0, lenr = 0;
	int msgcnt, calls;
	R_xlen_t index;
	_M(MPI_Recv(cmdr, CMDLINESZ, MPI_INT, wkr, RHPC_CTRL_TAG, comm, &stat));
	GET_CMD(cmdr, &cmdmainr, &cmdsubr, &cntr, &modr, &dummy_quote);
	lenr = RHPC_SPLIT_SIZE * cntr + modr;
	REPROTECT(indata=allocVector(RAWSXP,lenr),indata_ix);
	
	msgcnt = cntr+((modr)?1:0);
	calls=0;
	{
	  MPI_Request *request = R_Calloc(msgcnt, MPI_Request);
	  MPI_Status  *status  = R_Calloc(msgcnt, MPI_Status);
	  for(i=0;i<cntr;i++){
	    _M(MPI_Irecv(RAW(indata)+RHPC_SPLIT_SIZE*i, 
			 (int)RHPC_SPLIT_SIZE, MPI_CHAR,
			 wkr, TAGCAL(i),    comm, &request[calls]));
	    calls++;
	  }
	  if ( modr != 0 ){
	    _M(MPI_Irecv(RAW(indata)+RHPC_SPLIT_SIZE*cntr,
			 (int)modr,            MPI_CHAR,
			 wkr, TAGCAL(cntr), comm, &request[calls]));
	    calls++;
	  }
	  _M(MPI_Waitall(calls, request, status));
	  R_Free(status);
	  R_Free(request);
	}
	
	REPROTECT(uns  = Rhpc_unserialize(indata), uns_ix);
	index = (R_xlen_t)REAL(VECTOR_ELT(uns,0))[0];
	REPROTECT(ans = VECTOR_ELT(uns,1),ans_ix);
	SET_VECTOR_ELT(outlist, index, ans);
	worker_status[wkr] = WORKER_IDLE;
	active_workers--;
	DPRINT("finish rank=%d ix=%ld\n", wkr, index);
      }
    }
  }
  Rhpc_mpi_lapply_LB_exit(num_procs, comm);
  UNPROTECT(9);
  R_Free(worker_status);
  pop_policy();
  return (_CHK(outlist));
}