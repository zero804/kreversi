/* Yo Emacs, this -*- C++ -*-
 *******************************************************************
 *******************************************************************
 *
 *
 * KREVERSI
 *
 *
 *******************************************************************
 *
 * A Reversi (or sometimes called Othello) game
 *
 *******************************************************************
 *
 * created 1997 by Mario Weilguni <mweilguni@sime.com>
 *
 *******************************************************************
 *
 * This file is part of the KDE project "KREVERSI"
 *
 * KREVERSI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * KREVERSI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KREVERSI; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *******************************************************************
 */

#ifndef __PLAYSOUND__H__
#define __PLAYSOUND__H__

#include "misc.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Configure now does not set this #define anymore, as libmediatool
// is on any KDE system. So I fake it here
#define HAVE_MEDIATOOL

#ifdef HAVE_MEDIATOOL
#include <mediatool.h>
#include <kaudio.h>
#endif

#include <qglobal.h>

bool initAudio();
bool doneAudio();
bool audioOK();
bool playSound(const char *);
bool soundSync();
bool syncPlayAudio(const char *);

#endif
