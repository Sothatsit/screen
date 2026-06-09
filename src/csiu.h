/* Copyright (c) 2026
 *      Padraig Lamont
 *
 * This file is part of GNU screen.
 *
 * GNU screen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU screen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, see
 * https://www.gnu.org/licenses/.
 *
 ****************************************************************
 */

#ifndef SCREEN_CSIU_H
#define SCREEN_CSIU_H

#include <stddef.h>

#define CSIU_MAX_SEQUENCE 128

typedef struct {
	unsigned char pending[CSIU_MAX_SEQUENCE];
	size_t pending_len;
} CsiuParser;

void csiu_reset(CsiuParser *);
size_t csiu_pending_size(const CsiuParser *);
const unsigned char *csiu_pending_data(const CsiuParser *);
size_t csiu_translate(CsiuParser *, unsigned char **, size_t);

#endif
