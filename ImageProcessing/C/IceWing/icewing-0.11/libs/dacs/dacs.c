/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

#include <stdio.h>
#include "dacs.h"

#define ERROR	" DACS support disabled in this iceWing installation !\n"

void dfree (void *ptr)
{
	fprintf (stderr, "! dfree:"ERROR);
}

void *dalloc (size_t bytes)
{
	fprintf (stderr, "! dalloc:"ERROR);
	return NULL;
}

void *dzalloc (size_t bytes)
{
	fprintf (stderr, "! dzalloc:"ERROR);
	return NULL;
}

NDRstatus_t _ndr__enter (NDRS *ndrs, void **obj, size_t size)
{
	fprintf (stderr, "! _ndr__enter:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t _ndr__leave (NDRS *ndrs, void **obj)
{
	fprintf (stderr, "! _ndr__leave:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_free (void **obj, NDRfunction_t *ndr_proc)
{
	fprintf (stderr, "! ndr_free:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_open (NDRS *ndrs, void **obj)
{
	fprintf (stderr, "! ndr_open:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_pair (NDRS *ndrs)
{
	fprintf (stderr, "! ndr_pair:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_nil (NDRS *ndrs)
{
	fprintf (stderr, "! ndr_nil:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_float (NDRS *ndrs, float *f)
{
	fprintf (stderr, "! ndr_float:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_list (NDRS *ndrs, int *len,
					  NDRstatus_t (*ndr_elem)(), void ***data)
{
	fprintf (stderr, "! ndr_list:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_string (NDRS *ndrs, char **s)
{
	fprintf (stderr, "! ndr_string:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_hyper (NDRS *ndrs, int *ord,
					   NDRtype_t *el_type, void ***data, ...)
{
	fprintf (stderr, "! ndr_hyper:"ERROR);
	return NDR_ERROR;
}

NDRstatus_t ndr_int (NDRS *ndrs, int *i)
{
	fprintf (stderr, "! ndr_int:"ERROR);
	return NDR_ERROR;
}

DACSentry_t *dacs_register (char *myname)
{
	fprintf (stderr, "! dacs_register:"ERROR);
	return NULL;
}

void dacs_unregister (DACSentry_t *dacs_entry)
{
	fprintf (stderr, "! dacs_unregister:"ERROR);
}

DACSstatus_t dacs_register_stream (DACSentry_t *dacs_entry, char *stream_name,
								   NDRfunction_t *ndr_function)
{
	fprintf (stderr, "! dacs_register_stream:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_register_function (DACSentry_t *dacs_entry,
									 char *function_name,
									 DACSfunction_t *function,
									 NDRfunction_t *ndr_in_function,
									 NDRfunction_t *ndr_out_function,
									 void (*out_kill_function)(void*),
									 void *client_data)
{
	fprintf (stderr, "! dacs_register_function:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t _dacs_recv_message (DACSentry_t *dacs_entry,
								 char *channel,
								 DACSblockmode_t block,
								 DACSmessage_t **message)
{
	fprintf (stderr, "! _dacs_recv_message:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t _dacs_destroy_message (DACSmessage_t *message)
{
	fprintf (stderr, "! _dacs_destroy_message:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_order_stream_with_retry (DACSentry_t *dacs_entry,
										   char *stream_name,
										   char *channel_name,
										   int synclevel,
										   int max_retry)
{
	fprintf (stderr, "! dacs_order_stream_with_retry:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_sync_stream (DACSentry_t *dacs_entry, char *stream_name,
							   int synclevel)
{
	fprintf (stderr, "! dacs_sync_stream:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_update_stream (DACSentry_t *dacs_entry, char *stream_name,
								 void *obj)
{
	fprintf (stderr, "! dacs_update_stream:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_serv_functions_async (DACSentry_t *dacs_entry)
{
	fprintf (stderr, "! dacs_serv_functions_async:"ERROR);
	return D_GEN_ERROR;
}

DACSstatus_t dacs_recv (DACSentry_t *dacs_entry, char *channel,
						NDRfunction_t *ndr_function, void **buffer)
{
	fprintf (stderr, "! dacs_recv:"ERROR);
	return D_GEN_ERROR;
}
