/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Dummy implementation of some DACS and SFB functions to enable compiling
 * iceWing without a DACS or SFB installation.
 *
 * Attention: All these functions only display an error message!
 */

#ifndef iw_dacs_H
#define iw_dacs_H

#include <pthread.h>

#ifdef leave
/* Undef 'leave' defined in pthread.h on alpha, gtk uses it */
#undef leave
#endif

typedef void NDRS;
typedef enum {
	NDR_SUCCESS,
	NDR_ERROR,
	NDR_EOS
} NDRstatus_t;

typedef enum {
	NDRunknown	= 0,
	NDRnil		= 1,
	NDRbyte		= 16,
	NDRchar		= 17,
	NDRshort	= 18,
	NDRint		= 19,
	NDRfloat	= 20,
	NDRdouble	= 21,
	NDRstring	= 22,
	NDRsymbol	= 23,
	NDRvector	= 32,
	NDRlist		= 33,
	NDRhyper	= 64,
	NDRpair		= 65
} NDRtype_t;

typedef NDRstatus_t (NDRfunction_t)(NDRS *, void **);

#define DACS_MAXNAMELEN			64

typedef enum {
	D_OK,
	D_GEN_ERROR,
	D_NDR_ERROR,
	D_NO_DATA,
	D_SYNC
} DACSstatus_t;

typedef enum {
	DACS_BLOCK
} DACSblockmode_t;

typedef void DACSentry_t;
typedef char* (DACSfunction_t)(void* in, void* out, void* client_data);

typedef struct DACSmessage_t {
	NDRS *ndrs;
} DACSmessage_t;

#ifdef __cplusplus
extern "C" {
#endif

DACSentry_t *dacs_register (char *myname);
void dacs_unregister (DACSentry_t *dacs_entry);
DACSstatus_t dacs_register_stream (DACSentry_t *dacs_entry, char *stream_name,
								   NDRfunction_t *ndr_function);
DACSstatus_t dacs_register_function (DACSentry_t *dacs_entry,
									 char *function_name,
									 DACSfunction_t *function,
									 NDRfunction_t *ndr_in_function,
									 NDRfunction_t *ndr_out_function,
									 void (*out_kill_function)(void*),
									 void *client_data);
DACSstatus_t _dacs_recv_message (DACSentry_t *dacs_entry,
								 char *channel,
								 DACSblockmode_t block,
								 DACSmessage_t **message);
DACSstatus_t _dacs_destroy_message (DACSmessage_t *message);
DACSstatus_t dacs_order_stream_with_retry (DACSentry_t *dacs_entry,
										   char *stream_name,
										   char *channel_name,
										   int synclevel,
										   int max_retry);
DACSstatus_t dacs_sync_stream (DACSentry_t *dacs_entry, char *stream_name,
							   int synclevel);
DACSstatus_t dacs_update_stream (DACSentry_t *dacs_entry, char *stream_name,
								 void *obj);
DACSstatus_t dacs_serv_functions_async (DACSentry_t *dacs_entry);
DACSstatus_t dacs_recv (DACSentry_t *dacs_entry, char *channel,
						NDRfunction_t *ndr_function, void **buffer);

void dfree (void *ptr);
void *dalloc (size_t bytes);
void *dzalloc (size_t bytes);

NDRstatus_t _ndr__enter (NDRS *ndrs, void **obj, size_t size);
NDRstatus_t _ndr__leave (NDRS *ndrs, void **obj);

NDRstatus_t ndr_open (NDRS *ndrs, void **obj);
NDRstatus_t ndr_pair (NDRS *ndrs);
NDRstatus_t ndr_nil (NDRS *ndrs);

NDRstatus_t ndr_free (void **obj, NDRfunction_t *ndr_proc);
NDRstatus_t ndr_float (NDRS *ndrs, float *f);
NDRstatus_t ndr_list (NDRS *ndrs, int *len,
					  NDRstatus_t (*ndr_elem)(), void ***data);
NDRstatus_t ndr_string (NDRS *ndrs, char **s);
NDRstatus_t ndr_hyper (NDRS *ndrs, int *ord,
					   NDRtype_t *el_type, void ***data, ...);
NDRstatus_t ndr_int (NDRS *ndrs, int *i);

#ifdef __cplusplus
}
#endif

#endif /* iw_dacs_H */
