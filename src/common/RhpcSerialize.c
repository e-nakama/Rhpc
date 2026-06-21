/*
 *
 * Updated based on r-base-4.6.0, src/main/serialize.c 
 *
 */
/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995--2024  The R Core Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

#include <stdlib.h>
#include <string.h>
#include "Rhpc.h"

/*
 * Persistent Memory Streams
 */

typedef struct membuf_st {
    R_xlen_t size;
    R_xlen_t count;
    unsigned char *buf;
    int overflow;
} *membuf_t;

#ifdef RHPC_ALTVEC
/* ALTREP Class definition */
R_altrep_class_t Rhpc_SerializedRaw_class;
int Rhpc_altrep_initialized = 0;

static R_xlen_t SerializedRaw_length(SEXP x) {
    membuf_t mb = (membuf_t)R_ExternalPtrAddr(R_altrep_data1(x));
    return mb->count;
}

static void *SerializedRaw_dataptr(SEXP x, Rboolean writeable) {
    membuf_t mb = (membuf_t)R_ExternalPtrAddr(R_altrep_data1(x));
    return mb->buf;
}

static const void *SerializedRaw_dataptr_or_null(SEXP x) {
    // This method is called when R needs a pointer to the data, but it might not be writeable.
    membuf_t mb = (membuf_t)R_ExternalPtrAddr(R_altrep_data1(x));
    return mb->buf;
}

static void membuf_finalizer(SEXP ptr) {
    membuf_t mb = (membuf_t)R_ExternalPtrAddr(ptr);
    if (mb) {
        if (mb->buf) free(mb->buf);
        free(mb);
        R_SetExternalPtrAddr(ptr, NULL);
        R_ClearExternalPtr(ptr); // Clear the external pointer to prevent double-free issues
    }
}

/* Class initialization (must be called when the package is loaded) */
void Rhpc_init_serialize_altrep(DllInfo *dll) {
    if (Rhpc_altrep_initialized) return;
    R_altrep_class_t cls = R_make_altraw_class("Rhpc_serialized_raw", "Rhpc", dll);
    Rhpc_SerializedRaw_class = cls;
    R_set_altrep_Length_method(cls, SerializedRaw_length); // This is correct for all altrep types
    R_set_altvec_Dataptr_method(cls, SerializedRaw_dataptr);
    R_set_altvec_Dataptr_or_null_method(cls, SerializedRaw_dataptr_or_null);
    Rhpc_altrep_initialized = 1;
}

static void init_altrep_lazy(void) {
    if (!Rhpc_altrep_initialized) {
        Rhpc_init_serialize_altrep(NULL);
    }
}
#endif

/* Based on R 4.x serialization defaults */
static int defaultSerializeVersion(void)
{
    static int dflt = -1;

    if (dflt < 0) {
	char *valstr = getenv("R_DEFAULT_SERIALIZE_VERSION");
	int val = (valstr != NULL) ? atoi(valstr) : 3;
	if (val >= 2 && val <= 3)
	    dflt = val;
	else
	    dflt = 3; /* the modern default since R 3.5.0 */
    }
    return dflt;
}

#define MAXELTSIZE 8192
#define INCR MAXELTSIZE
static void resize_buffer(membuf_t mb, R_xlen_t needed)
{
    unsigned char *tmp;
    if (needed < 0 || needed > R_XLEN_T_MAX)
	error("serialization is too large to store in a raw vector");

    if (needed < 10000000) /* ca 10MB */
	needed = (1+2*needed/INCR) * INCR;
    else 
	needed = (R_xlen_t)((1+1.5*(double)needed/INCR) * INCR);

    tmp = realloc(mb->buf, needed);
    if (tmp == NULL) {
	free(mb->buf); mb->buf = NULL;
	error("cannot allocate buffer");
    } else mb->buf = tmp;
    mb->size = needed;
}

static void OutCharMem(R_outpstream_t stream, int c)
{
    membuf_t mb = stream->data;
    if (mb->count >= mb->size)
	resize_buffer(mb, mb->count + 1);
    mb->buf[mb->count++] = (unsigned char) c;
}

static void OutCharMem_onlysize(R_outpstream_t stream, int c)
{
    membuf_t mb = stream->data;
    mb->count++;
}

static void OutCharMem_norealloc(R_outpstream_t stream, int c)
{
    membuf_t mb = stream->data;
    mb->buf[mb->count++] = (unsigned char) c;
}

static void OutBytesMem(R_outpstream_t stream, void *buf, int length)
{
    membuf_t mb = stream->data;
    R_xlen_t needed = mb->count + (R_xlen_t) length;
#ifndef LONG_VECTOR_SUPPORT
    if (needed > INT_MAX)
	error("serialization is too large to store in a raw vector");
#endif
    if (needed > mb->size) resize_buffer(mb, needed);
    memcpy(mb->buf + mb->count, buf, length);
    mb->count = needed;
}

static void OutBytesMem_onlysize(R_outpstream_t stream, void *buf, int length)
{
    membuf_t mb = stream->data;
    R_xlen_t needed = mb->count + (R_xlen_t) length;
    mb->count = needed;
}

static void OutBytesMem_norealloc(R_outpstream_t stream, void *buf, int length)
{
    membuf_t mb = stream->data;
    R_xlen_t needed = mb->count + (R_xlen_t) length;
#ifndef LONG_VECTOR_SUPPORT
    if (needed > INT_MAX)
	error("serialization is too large to store in a raw vector");
#endif
    memcpy(mb->buf + mb->count, buf, length);
    mb->count = needed;
}

static int InCharMem(R_inpstream_t stream)
{
    membuf_t mb = stream->data;
    if (mb->count >= mb->size)
	error("read error: buffer overflow");
    return mb->buf[mb->count++];
}

static void InBytesMem(R_inpstream_t stream, void *buf, int length)
{
    membuf_t mb = stream->data;
    if (length > 0 && mb->count + (R_xlen_t) length > mb->size)
	error("read error: buffer overflow");
    memcpy(buf, mb->buf + mb->count, length);
    mb->count += length;
}

static void InitMemInPStream(R_inpstream_t stream, membuf_t mb,
			     void *buf, R_xlen_t length,
			     SEXP (*phook)(SEXP, SEXP), SEXP pdata)
{
    mb->count = 0;
    mb->size = length;
    mb->buf = buf;
    R_InitInPStream(stream, (R_pstream_data_t) mb, R_pstream_any_format,
		    InCharMem, InBytesMem, phook, pdata);
}

static void InitMemOutPStream(R_outpstream_t stream, membuf_t mb,
			      R_pstream_format_t type, int version,
			      SEXP (*phook)(SEXP, SEXP), SEXP pdata)
{
    mb->count = 0;
    mb->size = 0;
    mb->buf = NULL;
    R_InitOutPStream(stream, (R_pstream_data_t) mb, type, version,
		     OutCharMem, OutBytesMem, phook, pdata);
}

static void InitMemOutPStream_onlysize(R_outpstream_t stream, membuf_t mb,
				       R_pstream_format_t type, int version,
				       SEXP (*phook)(SEXP, SEXP), SEXP pdata)
{
    mb->count = 0;
    mb->size = 0;
    mb->buf = NULL;
    R_InitOutPStream(stream, (R_pstream_data_t) mb, type, version,
		     OutCharMem_onlysize, OutBytesMem_onlysize, phook, pdata);
}

static void InitMemOutPStream_norealloc(R_outpstream_t stream, membuf_t mb,
					R_pstream_format_t type, int version,
					SEXP (*phook)(SEXP, SEXP), SEXP pdata)
{
    mb->count = 0;
    /* mb->size and mb->buf from upper function */
    R_InitOutPStream(stream, (R_pstream_data_t) mb, type, version,
		     OutCharMem_norealloc, OutBytesMem_norealloc, phook, pdata);
}

static void free_mem_buffer(void *data)
{
    membuf_t mb = data;
    if (mb->buf != NULL) {
        unsigned char *buf = mb->buf;
        mb->buf = NULL;
        free(buf);
    }
}

static SEXP CloseMemOutPStream(R_outpstream_t stream)
{
    SEXP val;
    membuf_t mb = stream->data;

    PROTECT(val = allocVector(RAWSXP, mb->count));
    memcpy(RAW(val), mb->buf, mb->count);
    free_mem_buffer(mb);
    UNPROTECT(1);
    return val;
}

static SEXP CloseMemOutPStream_onlysize(R_outpstream_t stream)
{
    SEXP val;
    membuf_t mb = stream->data;

    PROTECT(val = allocVector(REALSXP, 1));
    REAL(val)[0] = mb->count;
    UNPROTECT(1);
    return val;
}

/** ---- **/

/* Context structure for R_UnwindProtect */
struct serialize_context {
    SEXP object;
    R_outpstream_t out;
    membuf_t mb;
};

static R_xlen_t Rhpc_get_serialize_size_internal(SEXP object)
{
    struct R_outpstream_st out;
    struct membuf_st mbs;

    InitMemOutPStream_onlysize(&out, &mbs, R_pstream_binary_format, 
                               defaultSerializeVersion(), NULL, R_NilValue);
    R_Serialize(object, &out);
    return mbs.count;
}

SEXP Rhpc_serialize(SEXP object)
{
#ifdef RHPC_ALTVEC
    struct R_outpstream_st out;
    membuf_t mb;
    SEXP val;

    init_altrep_lazy();

    mb = (membuf_t) malloc(sizeof(struct membuf_st));
    if (mb == NULL) error("cannot allocate membuf");
    mb->buf = NULL; mb->size = 0; mb->count = 0;

    R_InitOutPStream(&out, (R_pstream_data_t) mb, R_pstream_binary_format, 
                     defaultSerializeVersion(), OutCharMem, OutBytesMem, NULL, R_NilValue);

    R_Serialize(object, &out);

    /* Pass to R as an ALTREP object (no copy) */
    SEXP mbeptr;
    PROTECT(mbeptr = R_MakeExternalPtr(mb, R_NilValue, R_NilValue));
    R_RegisterCFinalizer(mbeptr, membuf_finalizer);
    
    val = R_new_altrep(Rhpc_SerializedRaw_class, mbeptr, R_NilValue);
    UNPROTECT(1); /* mbeptr */
    return val;
#else
    struct R_outpstream_st out;
    R_pstream_format_t type;
    int version;
    struct membuf_st mbs;
    SEXP val;

    version = defaultSerializeVersion();
    type = R_pstream_binary_format;

    InitMemOutPStream(&out, &mbs, type, version, NULL, R_NilValue);
    R_Serialize(object, &out);
    val =  CloseMemOutPStream(&out);
    
    return val;
#endif
}

SEXP Rhpc_serialize_onlysize(SEXP object)
{
    SEXP val;
    R_xlen_t count = Rhpc_get_serialize_size_internal(object);
    PROTECT(val = allocVector(REALSXP, 1));
    REAL(val)[0] = (double)count;
    UNPROTECT(1);
    return val;  
}

SEXP Rhpc_serialize_norealloc(SEXP object)
{
    struct R_outpstream_st out;
    R_pstream_format_t type = R_pstream_binary_format;
    int version = defaultSerializeVersion();
    struct membuf_st mbs;
    SEXP val;

    mbs.size = Rhpc_get_serialize_size_internal(object);
    PROTECT(val = allocVector(RAWSXP, mbs.size));
    mbs.buf  = RAW(val);
    mbs.overflow = 0;
    
    InitMemOutPStream_norealloc(&out, &mbs, type, version, NULL, R_NilValue);
    R_Serialize(object, &out);

    UNPROTECT(1);
    return val;
}

SEXP Rhpc_unserialize(SEXP object)
{
   struct R_inpstream_st in;

    /* We might want to read from a long raw vector */
    struct membuf_st mbs;

    if(TYPEOF(object) == RAWSXP){
	void *data = RAW(object);
	R_xlen_t length = XLENGTH(object);
	InitMemInPStream(&in, &mbs, data, length, NULL, NULL);
	return R_Unserialize(&in);
    }
    error("can't unserialize object");
    return R_UnboundValue;
}
