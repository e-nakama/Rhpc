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
#include "RhpcMPIWorkerCall.h"

SEXP Rhpc_mpi_worker_call(SEXP cl, SEXP args, SEXP actioncode, SEXP usequote)
{
  int action=INTEGER(actioncode)[0];
  R_xlen_t i;
  MPI_Comm comm;
  int num_procs;
  SEXP out, l_out=R_NilValue;
  R_xlen_t szi, cnti, modi;
  int cmd[CMDLINESZ], dummy_cmd[CMDLINESZ] = {0}, *cmds;
  SEXP inlist, inlista;
  R_xlen_t *cnts, *mods;
  int total_recv_calls;
  PROTECT_INDEX ix0;
  MPI_Request *request;
  MPI_Status  *status;
  int call_idx;
  SEXP outlist;

  if(TYPEOF(cl)!=EXTPTRSXP) error("it's not MPI_Comm external pointer\n");
  comm = SXP2COMM(cl);
  _M(MPI_Comm_size(comm, &num_procs));

  if(finalize){ warning("Rhpc were already finalized."); return(R_NilValue); }
  if(!initialize){ warning("Rhpc not initialized."); return(R_NilValue); }

  push_policy();
  PROTECT(l_out = R_NilValue);
  PROTECT(out=Rhpc_serialize_norealloc(args));

  szi = xlength(out);
  Rhpc_get_chunks(szi, &cnti, &modi);

  int cmd_main = (action == 0) ? CMD_NAME_WORKERCALL_NORET : 
                 (action == 2) ? CMD_NAME_WORKERCALL_EXPORT : CMD_NAME_WORKERCALL_RET;
  
  SET_CMD(cmd, cmd_main, SUBCMD_NORMAL, cnti, modi, INTEGER(usequote)[0]);
  _M(MPI_Bcast(cmd, CMDLINESZ, MPI_INT, 0, comm));

  for (i = 0; i < cnti; i++) _M(MPI_Bcast(RAW(out) + RHPC_SPLIT_SIZE * i, (int)RHPC_SPLIT_SIZE, MPI_CHAR, 0, comm));
  if (modi != 0) _M(MPI_Bcast(RAW(out) + RHPC_SPLIT_SIZE * cnti, (int)modi, MPI_CHAR, 0, comm));

  if(action==0){
      SEXP nillist;
      PROTECT(nillist=allocVector(VECSXP, num_procs - 1));
      for(i = 1; i < num_procs; i++) SET_VECTOR_ELT(nillist, i - 1, R_NilValue);
      UNPROTECT(3);
      pop_policy();
      return(nillist);
  }

  cmds = R_Calloc(num_procs * CMDLINESZ, int);
  _M(MPI_Gather(dummy_cmd, CMDLINESZ, MPI_INT, cmds, CMDLINESZ, MPI_INT, 0, comm));

  cnts = R_Calloc(num_procs, R_xlen_t);
  mods = R_Calloc(num_procs, R_xlen_t);
  total_recv_calls = 0;

  PROTECT(inlist=allocVector(VECSXP, num_procs - 1));
  inlista=R_NilValue;
  PROTECT_WITH_INDEX(inlista,&ix0);

  for(i = 1; i < num_procs; i++){
    int dummy_main, dummy_sub, dummy_quo;
    GET_CMD(cmds + CMDLINESZ * i, &dummy_main, &dummy_sub, &cnts[i], &mods[i], &dummy_quo); 
    R_xlen_t total_len = cnts[i] * RHPC_SPLIT_SIZE + mods[i];
    total_recv_calls += (int)cnts[i] + (mods[i] ? 1 : 0);
    REPROTECT(inlista = allocVector(RAWSXP, total_len), ix0);
    SET_VECTOR_ELT(inlist, i - 1, inlista);
  }

  request = R_Calloc(total_recv_calls, MPI_Request);
  status  = R_Calloc(total_recv_calls, MPI_Status);
  call_idx = 0;

  for(i = 1; i < num_procs; i++){
    SEXP current_raw = VECTOR_ELT(inlist, i - 1);
    for(R_xlen_t j = 0; j < cnts[i]; j++){
      _M(MPI_Irecv(RAW(current_raw) + RHPC_SPLIT_SIZE * j, (int)RHPC_SPLIT_SIZE, MPI_CHAR, (int)i, TAGCAL(j), comm, &request[call_idx++]));
    }
    if (mods[i] != 0) {
      _M(MPI_Irecv(RAW(current_raw) + RHPC_SPLIT_SIZE * cnts[i], (int)mods[i], MPI_CHAR, (int)i, TAGCAL(cnts[i]), comm, &request[call_idx++]));
    }
  }
  _M(MPI_Waitall(call_idx, request, status));

  R_Free(request);
  R_Free(status);

  {
    PROTECT_INDEX ix1, ix2;
    SEXP un, l_un;
    PROTECT(outlist=allocVector(VECSXP, num_procs - 1));
    PROTECT_WITH_INDEX(l_un=R_NilValue,&ix1);
    PROTECT_WITH_INDEX(  un=R_NilValue,&ix2);
    for(i = 1; i < num_procs; i++){
      REPROTECT(un = Rhpc_unserialize(VECTOR_ELT(inlist, i - 1)), ix2);
      SET_VECTOR_ELT(outlist, i - 1, un);
    }
  }
  UNPROTECT(7);

  R_Free(cmds);
  R_Free(cnts);
  R_Free(mods);

  pop_policy();
  return(_CHK(outlist));
}
