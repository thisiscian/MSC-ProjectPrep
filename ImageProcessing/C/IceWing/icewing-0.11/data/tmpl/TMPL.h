/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*********************************************************************
  A small iceWing plugin as a starting point for own plugins.
  More information about iceWing can be found in the documentation
  icewing.pdf and in the installed header files.
*********************************************************************/

#ifndef TMPLlower_H
#define TMPLlower_H

#include "gui/Grender.h"
#include "main/plugin_cxx.h"

namespace ICEWING
{

class TMPL : public Plugin
{
  public:
	TMPL (char *name) : Plugin(name) {};
	~TMPL ();
						
	void Init (grabParameter *para, int argc, char **argv);
	int  InitOptions ();
	bool Process (char *ident, plugData *data);

  private:
	void help (void);

	/** Declare other parameters here (e.g. for iceWing GUI options). */
	int value;
	prevBuffer *b_image;
};

} // namespace ICEWING

#endif // TMPLlower_H
