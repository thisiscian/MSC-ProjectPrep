/* -*- mode: C; tab-width: 4; c-basic-offset: 4; -*- */

/*
 * Author: Frank Loemker
 *
 * Copyright (C) 1999-2009
 * Frank Loemker, Applied Computer Science, Faculty of Technology,
 * Bielefeld University, Germany
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

#ifndef iw_mainloop_H
#define iw_mainloop_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
  Show results of the time_() calls and call any functions set with
  plug_function_register("mainCallback"). E.g. the grab plugin allows
  to stop/slow down the processing. Should be called periodically.
*********************************************************************/
void iw_mainloop_cyclic (void);

/*********************************************************************
  PRIVATE: Main loop for calling all plugins.
*********************************************************************/
void iw_mainloop_start (grabParameter *para);

#ifdef __cplusplus
}
#endif

#endif /* iw_mainloop_H */
