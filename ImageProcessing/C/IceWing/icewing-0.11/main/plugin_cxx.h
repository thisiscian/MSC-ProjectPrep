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
 * Contains the definition of the ICEWing::Plugin interface. Any customized C++
 * plugin for iceWing must be implemented against this interface. Do not perform
 * any changes to this file as they are going to affect all existing C++ plugin
 * implementations! */

#ifndef iw_plugin_cxx_H
#define iw_plugin_cxx_H

#include "plugin.h"

namespace ICEWING
{

class Plugin : public plugDefinition
{
  public:
	Plugin (char *name, int abi_version = PLUG_ABI_VERSION);
	virtual ~Plugin() {};

	virtual void Init (grabParameter *para, int argc, char **argv) = 0;
	virtual int  InitOptions () = 0;
	virtual bool Process (char *ident, plugData *data) = 0;
};

} /* namespace ICEWING */

#endif /* iw_plugin_cxx_H */
