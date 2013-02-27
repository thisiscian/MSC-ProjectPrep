/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker and Dirk Stoessel
 *
 * Copyright (C) 1999-2009
 * Applied Computer Science, Faculty of Technology, Bielefeld University, Germany
 *
 * This file is part of iceWing, a graphical plugin shell.
 *
 * iceWing is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * iceWing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

/** @file
 * Contains the definition of class MinPlugin which implements the
 * ICEWing::Plugin interface. MinPlugin demonstrates the implementation
 * of a most basic plugin. Its only activity is to write some information
 * about currently grabbed images to the standard output.
 * Its functionality is similar to the 'min' C plugin.
 */

#ifndef min_cxx_H
#define min_cxx_H

#include "main/plugin_cxx.h"

namespace ICEWING
{

class MinPlugin : public Plugin
{
  public:
	MinPlugin (char *name) : Plugin(name) {};
	~MinPlugin ();
						
	void Init (grabParameter *para, int argc, char **argv);
	int  InitOptions ();
	bool Process (char *ident, plugData *data);

  private:
	/** Declare other parameters here (e.g. for iceWing GUI options). */
};

} // namespace ICEWING

#endif // min_cxx_H
