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
#include "config.h"
#endif

#include "Rhpc.h"
#include <R.h>
#include <Rinternals.h>

SEXP Rhpc_enquote(SEXP arg)
{
  R_xlen_t i, len = xlength(arg);
  SEXP argq=R_NilValue;
  SEXP nm = getAttrib(arg, R_NamesSymbol);
  SEXP quote_sym;
  PROTECT_INDEX pqix;
  PROTECT(nm);
  PROTECT_WITH_INDEX(argq, &pqix);  
  argq = allocVector(VECSXP, len);
  REPROTECT(argq , pqix);
  quote_sym = install("quote");
  PROTECT(quote_sym);
  for(i=0;i<len;i++){
    SEXP ll, aa;
    PROTECT(aa = CONS(VECTOR_ELT(arg,i),R_NilValue));
    PROTECT(ll = LCONS(quote_sym,aa));
    SET_VECTOR_ELT(argq, i, ll);
    UNPROTECT(2);
  }
  setAttrib(argq, R_NamesSymbol, duplicate(nm));
  UNPROTECT(3);
  return(argq);
}

#define SPLITSIZEIX(LEN,SPLIT,IX) (LEN/SPLIT+((IX<(LEN%SPLIT))?1:0)) 

SEXP Rhpc_splitList(SEXP orgList, SEXP splitNum)
{
  R_xlen_t spnum, sz, i, j;
  SEXP outList, origListNm, origListClass;
  PROTECT_INDEX outList_ix;

  if(TYPEOF(orgList) != VECSXP ) return (orgList);

  spnum = INTEGER(splitNum)[0];
  sz = xlength(orgList);

  outList = allocVector(VECSXP,spnum);
  PROTECT_WITH_INDEX(outList, &outList_ix);

  PROTECT(origListNm = getAttrib(orgList, R_NamesSymbol));
  PROTECT(origListClass = getAttrib(orgList, R_ClassSymbol));

  for ( i=0; i< spnum; i++){
    SEXP work, workNm = R_NilValue;
    R_xlen_t chunk_sz = SPLITSIZEIX(sz, spnum, i);
    R_xlen_t k = 0;
    PROTECT(work = allocVector(VECSXP, chunk_sz));
    if (origListNm != R_NilValue) {
      PROTECT(workNm = allocVector(STRSXP, chunk_sz));
      for (j = i; j < sz; j += spnum, k++) {
        SET_VECTOR_ELT(work, k, VECTOR_ELT(orgList, j));
        SET_STRING_ELT(workNm, k, STRING_ELT(origListNm, j));
      }
      setAttrib(work, R_NamesSymbol, workNm);
      UNPROTECT(1);
    } else {
      for (j = i; j < sz; j += spnum, k++) {
        SET_VECTOR_ELT(work, k, VECTOR_ELT(orgList, j));
      }
    }
    if(origListClass != R_NilValue)
      setAttrib(work, R_ClassSymbol, origListClass);
    SET_VECTOR_ELT(outList, i, work);
    UNPROTECT(1);
  }
  UNPROTECT(3);
  return(outList);
}
