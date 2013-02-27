/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Some SFB definitions to enable compiling iceWing without
 * an SFB installation.
 */

#ifndef iw_sfb_iw_H
#define iw_sfb_iw_H

#include <dacs.h>

#ifndef SFB_H_INCLUDED

#define NDR_OPTIONAL

/* Image types which may occur in Bild_t.
 * These are 'orable' values to be able to put more than one
 * type in a Bild_t.
 */

#define SW_BILD		0x00000001
#define R_BILD		0x00000002
#define G_BILD		0x00000004
#define B_BILD		0x00000008
#define H_BILD		0x00000010
#define S_BILD		0x00000020
#define HSV_V_BILD	0x00000040
#define Y_BILD		0x00000080
#define U_BILD		0x00000100
#define YUV_V_BILD	0x00000200
#define COLOR_BILD	0x00000400
#define EDGE_BILD	0x00000800

/* Color models for 'EchtFarb_t' */
#define FARB_MODELL_UNDEF	(-1)
#define FARB_MODELL_SW		0
#define FARB_MODELL_RGB		1
#define FARB_MODELL_YUV		2
#define FARB_MODELL_HSV		3

typedef enum { /* Possible colors for objects */
	hintergrund = 0,
	weiss,
	rot,
	gelb,
	orange,
	blau,
	gruen,
	violett,
	holz,
	elfenbein,
	schwarz,
	/* Attention: gap! */
	undef_Farbe = 0xf,
	MAX_FARB
} Farb_t;

typedef enum {
	av_head_l,
	av_head_r,
	st_head_l,
	st_head_r,
	hand,
	hand_position,
	MAX_KAMERA
} Kamera_t;

typedef enum {
	datacube,
	sequenzanlage,
	MAX_GERAET
} Geraet_t;

typedef enum {
	SKK_undef_HypTyp,
	SKK_Bild,
	SKK_Gruppier,
	SKK_Region,
	SKK_Objekt2D,
	SKK_Objekt3D,
	SKK_PartOfSpeech,
	SKK_HYPOTHESENTYP_MAX
} HypothesenTyp_t;

typedef enum {
	linie,
	ellipse,
	MAX_KONTURTYP
} Konturtyp_t;

typedef enum {
	SKK_matchtyp_undef,
	SKK_matchtyp_struct,
	SKK_matchtyp_boundary,
	SKK_MATCHTYP_MAX
} SKKmatchtyp_t;

typedef enum {
	SKK_seite_undef,
	SKK_seite_links,
	SKK_seite_rechts
} SKKseite_t;

typedef enum {
	segment,
	primitiv,
	kolinear,
	kurvilinear,
	naehe,
	parallelitaet,
	symmetrie,
	ribbon,
	geschlossenheit,
	MAX_GRUPPIERTYP
} Gruppiertyp_t;

typedef struct HypothesenKopf_t {
	char			*typ;
	char			*quelle;
	float			bewertung;
	unsigned int	sec;
	unsigned int	usec;
	int				sequenz_nr;
   NDR_OPTIONAL
	int				id;
	int				alter;
   NDR_OPTIONAL
	int				stabilitaet;
	HypothesenTyp_t typ_id;
	int				n_basis;	/* List of hypotheses */
	struct HypothesenKopf_t	**basis;
} HypothesenKopf_t;

typedef struct EchtFarb_t {
	int		modell;		/* Color model, see 'FARB_MODELL_???' */
	float	x;			/* Three color values according to the model */
	float	y;
	float	z;
} EchtFarb_t;

typedef struct Punkt_t {
	float 	x;
	float 	y;
} Punkt_t;

typedef struct Polar_t {
	float 	winkel;		/* ... in Radiant [-pi .. pi] */
	float 	radius;
} Polar_t;

typedef struct Koordinate_t {
	int 	x;
	int 	y;
} Koordinate_t;

typedef struct Polygon_t {
	int		n_punkte;
	Punkt_t	**punkt;
} Polygon_t;

typedef struct Bild_t {
	HypothesenKopf_t kopf;
	Kamera_t		kamera;
	Geraet_t		geraet;
	int				inhalt;		/* 'veroderte' defines, see above */
	Koordinate_t	gesamt;
	Koordinate_t	offset;

	int				anzahl;		/* Must match number of bits in 'inhalt' */
	Koordinate_t	delta;
	unsigned char	***bild;
} Bild_t;

typedef struct Kontur_t
{
	Konturtyp_t		typ;
	Punkt_t			start;
	Punkt_t			end;
	float			orientierung;
	float			laenge;
	Punkt_t			mitte;
	float			radius_a;
	float			radius_b;
	float			bogen;
   NDR_OPTIONAL
	float			start_winkel;
} Kontur_t;

typedef struct Gruppier_t
{
	Gruppiertyp_t	typ;
	int				n_konturen;
	Kontur_t		**kontur;
   NDR_OPTIONAL
	float			fehler;		/* Error of the approximation */
	int				n_approx;	/* List of contours, which ... */
	Kontur_t		**approx;	/* ... describe an approximation */
} Gruppier_t;

typedef struct PolygonMatch_t
{
	int				index;		/* ... of the polygon point */
	Punkt_t			punkt;		/* Projection point */
	float			abstand;
} PolygonMatch_t;

typedef struct GruppierMatch_t
{
	Gruppier_t		*gruppier;
	PolygonMatch_t	anfang;
	PolygonMatch_t	ende;
	float			abstand;
   NDR_OPTIONAL
	SKKmatchtyp_t	typ;			/* Type of the match */
	SKKseite_t		region_seite;	/* Mached region on the R/L side */
	int				region_ueberlapp; /* Contour overlaps region by N% */
	int				region_index;	/* Match belongs to region #N */
} GruppierMatch_t;

typedef struct Region_t {
	Punkt_t			schwerpunkt;
	Polygon_t		polygon;		/* Contour */
	Farb_t			farbe;			/* 1. and ... */
	Farb_t			farbe2;			/* .. 2. alternative of the color classification */
	int				pixelanzahl;
	int				umfang;			/*  #border pixel in 4-way neighborhood */
	Polar_t			hauptachse;
	float			exzentrizitaet;	/* 0: circle, 1: line */
	float			compactness;	/* 1: square <1: frayed border */
   NDR_OPTIONAL
	int				n_match;		/* List of grouppings */
	GruppierMatch_t	**match;
   NDR_OPTIONAL
	EchtFarb_t		echtfarbe;
   NDR_OPTIONAL
	int				n_einschluss;	/* List of "holes" in the region */
	Polygon_t		**einschluss;
} Region_t;

typedef struct RegionHyp_t {
	HypothesenKopf_t kopf;
	Kamera_t		kamera;
	Geraet_t		geraet;
	Region_t		*region;
} RegionHyp_t;

#ifdef __cplusplus
extern "C" {
#endif

NDRstatus_t ndr_Bild (NDRS *ndrs, Bild_t **obj);
NDRstatus_t ndr_RegionHyp (NDRS *ndrs, RegionHyp_t **obj);

#ifdef __cplusplus
}
#endif

#endif /* SFB_H_INCLUDED */

#endif /* iw_sfb_iw_H */
