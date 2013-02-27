/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Some SFB NDR encoding functions to enable compiling iceWing without
 * an SFB installation.
 */

#include "config.h"
#include "main/sfb_iw.h"

#define __IFSTATUSSUCCESS	if (status != NDR_SUCCESS) return(status);
#define __STATUS_PAIR		status = ndr_pair(ndrs);
#define __IFSTATUSEOS		if (status == NDR_EOS) return(_ndr__leave(ndrs, (void**) obj));
#define IPI		__IFSTATUSSUCCESS __STATUS_PAIR __IFSTATUSSUCCESS
#define IPI_OPT	__IFSTATUSSUCCESS __STATUS_PAIR __IFSTATUSEOS __IFSTATUSSUCCESS

static NDRstatus_t leave_decode (NDRS *ndrs, NDRstatus_t status, void **obj)
{
	__IFSTATUSSUCCESS;
	status = ndr_nil(ndrs);
	__IFSTATUSSUCCESS;
	return _ndr__leave(ndrs, obj);
}

static NDRstatus_t ndr_HypothesenKopf (NDRS *ndrs, HypothesenKopf_t **obj)
{
	NDRstatus_t status;

	status = ndr_open (ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return (NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(HypothesenKopf_t));
	__IFSTATUSSUCCESS;

	status = ndr_string(ndrs, &((*obj)->typ));
	IPI;
	status = ndr_string(ndrs, &((*obj)->quelle));
	IPI;
	status = ndr_float(ndrs, &((*obj)->bewertung));
	IPI;
	/* NDR does not support unsigned types, so live with the error */
	status = ndr_int(ndrs, (int*)&((*obj)->sec));
	IPI;
	/* NDR does not support unsigned types, so live with the error */
	status = ndr_int(ndrs, (int*)&((*obj)->usec));
	IPI;
	status = ndr_int(ndrs, &((*obj)->sequenz_nr));
	IPI_OPT;
	status = ndr_int(ndrs, &((*obj)->id));
	IPI;
	status = ndr_int(ndrs, &((*obj)->alter));
	IPI_OPT;
	status = ndr_int(ndrs, &((*obj)->stabilitaet));
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->typ_id));
	IPI;
	status = ndr_list(ndrs, &((*obj)->n_basis),
					  (NDRfunction_t *)ndr_HypothesenKopf, (void***)(void*)&((*obj)->basis));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_EchtFarb (NDRS *ndrs, EchtFarb_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(EchtFarb_t));
	__IFSTATUSSUCCESS;

	status = ndr_int(ndrs, &((*obj)->modell));
	IPI;
	status = ndr_float(ndrs, &((*obj)->x));
	IPI;
	status = ndr_float(ndrs, &((*obj)->y));
	IPI;
	status = ndr_float(ndrs, &((*obj)->z));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Punkt (NDRS *ndrs, Punkt_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Punkt_t));
	__IFSTATUSSUCCESS;

	status = ndr_float(ndrs, &((*obj)->x));
	IPI;
	status = ndr_float(ndrs, &((*obj)->y));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Polar (NDRS *ndrs, Polar_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Polar_t));
	__IFSTATUSSUCCESS;

	status = ndr_float(ndrs, &((*obj)->winkel));
	IPI;
	status = ndr_float(ndrs, &((*obj)->radius));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Koordinate (NDRS *ndrs, Koordinate_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Koordinate_t));
	__IFSTATUSSUCCESS;

	status = ndr_int(ndrs, &((*obj)->x));
	IPI;
	status = ndr_int(ndrs, &((*obj)->y));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Polygon (NDRS *ndrs, Polygon_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Polygon_t));
	__IFSTATUSSUCCESS;

	status = ndr_list(ndrs, &((*obj)->n_punkte),
					  (NDRfunction_t *)ndr_Punkt, (void***)(void*)&((*obj)->punkt));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Kontur (NDRS *ndrs, Kontur_t **obj)
{
	NDRstatus_t status;
	Punkt_t *punkt;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Kontur_t));
	__IFSTATUSSUCCESS;

	status = ndr_int(ndrs, (int*)(void*)&((*obj)->typ));
	IPI;
	punkt = &((*obj)->start);
	status = ndr_Punkt(ndrs, &punkt);
	IPI;
	punkt = &((*obj)->end);
	status = ndr_Punkt(ndrs, &punkt);
	IPI;
	status = ndr_float(ndrs, &((*obj)->orientierung));
	IPI;
	status = ndr_float(ndrs, &((*obj)->laenge));
	IPI;
	punkt = &((*obj)->mitte);
	status = ndr_Punkt(ndrs, &punkt);
	IPI;
	status = ndr_float(ndrs, &((*obj)->radius_a));
	IPI;
	status = ndr_float(ndrs, &((*obj)->radius_b));
	IPI;
	status = ndr_float(ndrs, &((*obj)->bogen));
	IPI_OPT;
	status = ndr_float(ndrs, &((*obj)->start_winkel));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Gruppier (NDRS *ndrs, Gruppier_t **obj)
{
	NDRstatus_t status;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Gruppier_t));
	__IFSTATUSSUCCESS;

	status = ndr_int(ndrs, (int*)(void*)&((*obj)->typ));
	IPI;
	status = ndr_list(ndrs, &((*obj)->n_konturen),
					  (NDRfunction_t *)ndr_Kontur, (void***)(void*)&((*obj)->kontur));
	IPI_OPT;
	status = ndr_float(ndrs, &((*obj)->fehler));
	IPI;
	status = ndr_list(ndrs, &((*obj)->n_approx),
					  (NDRfunction_t *)ndr_Kontur, (void***)(void*)&((*obj)->approx));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_PolygonMatch (NDRS *ndrs, PolygonMatch_t **obj)
{
	NDRstatus_t status;
	Punkt_t *punkt;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(PolygonMatch_t));
	__IFSTATUSSUCCESS;

	status = ndr_int(ndrs, &((*obj)->index));
	IPI;
	punkt = &((*obj)->punkt);
	status = ndr_Punkt(ndrs, &punkt);
	IPI;
	status = ndr_float(ndrs, &((*obj)->abstand));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_GruppierMatch (NDRS *ndrs, GruppierMatch_t **obj)
{
	NDRstatus_t status;
	PolygonMatch_t *match;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(GruppierMatch_t));
	__IFSTATUSSUCCESS;

	status = ndr_Gruppier(ndrs, &((*obj)->gruppier));
	IPI;
	match = &((*obj)->anfang);
	status = ndr_PolygonMatch(ndrs, &match);
	IPI;
	match = &((*obj)->ende);
	status = ndr_PolygonMatch(ndrs, &match);
	IPI;
	status = ndr_float(ndrs, &((*obj)->abstand));
	IPI_OPT;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->typ));
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->region_seite));
	IPI;
	status = ndr_int(ndrs, &((*obj)->region_ueberlapp));
	IPI;
	status = ndr_int(ndrs, &((*obj)->region_index));

	return leave_decode (ndrs, status, (void**) obj);
}

static NDRstatus_t ndr_Region (NDRS *ndrs, Region_t **obj)
{
	NDRstatus_t status;
	Punkt_t *punkt;
	Polygon_t *polygon;
	Polar_t *polar;
	EchtFarb_t *farb;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Region_t));
	__IFSTATUSSUCCESS;

	punkt = &((*obj)->schwerpunkt);
	status = ndr_Punkt(ndrs, &punkt);
	IPI;
	polygon = &((*obj)->polygon);
	status = ndr_Polygon(ndrs, &polygon);
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->farbe));
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->farbe2));
	IPI;
	status = ndr_int(ndrs, &((*obj)->pixelanzahl));
	IPI;
	status = ndr_int(ndrs, &((*obj)->umfang));
	IPI;
	polar = &((*obj)->hauptachse);
	status = ndr_Polar(ndrs, &polar);
	IPI;
	status = ndr_float(ndrs, &((*obj)->exzentrizitaet));
	IPI;
	status = ndr_float(ndrs, &((*obj)->compactness));
	IPI_OPT;
	status = ndr_list(ndrs, &((*obj)->n_match),
					  (NDRfunction_t *)ndr_GruppierMatch, (void***)(void*)&((*obj)->match));
	IPI_OPT;
	farb = &((*obj)->echtfarbe);
	status = ndr_EchtFarb(ndrs, &farb);
	IPI_OPT;
	status = ndr_list(ndrs, &((*obj)->n_einschluss),
					  (NDRfunction_t *)ndr_Polygon, (void***)(void*)&((*obj)->einschluss));

	return leave_decode (ndrs, status, (void**) obj);
}

NDRstatus_t ndr_Bild (NDRS *ndrs, Bild_t **obj)
{
	NDRstatus_t status;
	NDRtype_t type;
	int ord;
	HypothesenKopf_t *kopf;
	Koordinate_t *koordinate;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(Bild_t));
	__IFSTATUSSUCCESS;

	kopf = &((*obj)->kopf);
	status = ndr_HypothesenKopf(ndrs, &kopf);
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->kamera));
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->geraet));
	IPI;
	status = ndr_int(ndrs, &((*obj)->inhalt));
	IPI;
	koordinate = &((*obj)->gesamt);
	status = ndr_Koordinate(ndrs, &koordinate);
	IPI;
	koordinate = &((*obj)->offset);
	status = ndr_Koordinate(ndrs, &koordinate);
	IPI;
	type = NDRbyte;
	ord = 3;
	status = ndr_hyper(ndrs, &ord, &type,
					   (void***)(void*)&((*obj)->bild),
					   &((*obj)->anzahl),
					   &((*obj)->delta.y),
					   &((*obj)->delta.x));

	return leave_decode (ndrs, status, (void**) obj);
}

NDRstatus_t ndr_RegionHyp (NDRS *ndrs, RegionHyp_t **obj)
{
	NDRstatus_t status;
	HypothesenKopf_t *kopf;

	status = ndr_open(ndrs, (void**)obj);
	if (status == NDR_EOS)
	    return(NDR_SUCCESS);
	__IFSTATUSSUCCESS;

	status = _ndr__enter(ndrs, (void**) obj, sizeof(RegionHyp_t));
	__IFSTATUSSUCCESS;

	kopf = &((*obj)->kopf);
	status = ndr_HypothesenKopf(ndrs, &kopf);
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->kamera));
	IPI;
	status = ndr_int(ndrs, (int*)(void*)&((*obj)->geraet));
	IPI;
	status = ndr_Region(ndrs, &((*obj)->region));

	return leave_decode (ndrs, status, (void**) obj);
}
