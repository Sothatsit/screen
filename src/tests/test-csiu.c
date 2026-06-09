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

#include "../csiu.h"
#include "signature.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>

SIGNATURE_CHECK(csiu_reset, void, (CsiuParser *));
SIGNATURE_CHECK(csiu_pending_size, size_t, (const CsiuParser *));
SIGNATURE_CHECK(csiu_pending_data, const unsigned char *, (const CsiuParser *));
SIGNATURE_CHECK(csiu_translate, size_t, (CsiuParser *, unsigned char **, size_t));

static size_t translate(CsiuParser *parser, const char *input, unsigned char **out)
{
	static unsigned char space[CSIU_MAX_SEQUENCE + 256];
	unsigned char *buf = space + CSIU_MAX_SEQUENCE;
	size_t len = strlen(input);

	memcpy(buf, input, len);
	len = csiu_translate(parser, &buf, len);
	*out = buf;
	return len;
}

static void assert_bytes(const unsigned char *actual, size_t actual_len,
			 const unsigned char *expected, size_t expected_len)
{
	ASSERT(actual_len == expected_len);
	ASSERT(memcmp(actual, expected, expected_len) == 0);
}

static void make_long_text_sequence(char *buf)
{
	char *p = buf;

	p += sprintf(p, "\033[97;1;");
	for (int i = 0; i < 64; i++)
		p += sprintf(p, "%s128512", i == 0 ? "" : ":");
	sprintf(p, "u");
}

int main(void)
{
	CsiuParser parser = {{0}, 0};
	unsigned char *out;
	char long_text[512];
	size_t len;

	len = translate(&parser, "\033[97;5u", &out);
	assert_bytes(out, len, (const unsigned char *)"\001", 1);
	ASSERT(csiu_pending_size(&parser) == 0);

	len = translate(&parser, "\033[27u", &out);
	assert_bytes(out, len, (const unsigned char *)"\033", 1);

	len = translate(&parser, "\033[97;3u", &out);
	assert_bytes(out, len, (const unsigned char *)"\033a", 2);

	len = translate(&parser, "\033[97:65;6u", &out);
	assert_bytes(out, len, (const unsigned char *)"\001", 1);

	len = translate(&parser, "\033[0;;229u", &out);
	assert_bytes(out, len, (const unsigned char *)"\303\245", 2);

	len = translate(&parser, "\033[97;1:3u", &out);
	assert_bytes(out, len, (const unsigned char *)"", 0);

	len = translate(&parser, "\033[A", &out);
	assert_bytes(out, len, (const unsigned char *)"\033[A", 3);

	len = translate(&parser, "\033[<0;1;1M", &out);
	assert_bytes(out, len, (const unsigned char *)"\033[<0;1;1M", 9);

	len = translate(&parser, "\033[97;", &out);
	assert_bytes(out, len, (const unsigned char *)"", 0);
	ASSERT(csiu_pending_size(&parser) == 5);
	len = translate(&parser, "5u", &out);
	assert_bytes(out, len, (const unsigned char *)"\001", 1);
	ASSERT(csiu_pending_size(&parser) == 0);

	len = translate(&parser, "\033", &out);
	assert_bytes(out, len, (const unsigned char *)"", 0);
	ASSERT(csiu_pending_size(&parser) == 1);
	len = translate(&parser, "[27u", &out);
	assert_bytes(out, len, (const unsigned char *)"\033", 1);
	ASSERT(csiu_pending_size(&parser) == 0);

	len = translate(&parser, "\033[1;", &out);
	assert_bytes(out, len, (const unsigned char *)"", 0);
	ASSERT(csiu_pending_size(&parser) == 4);
	len = translate(&parser, "2A", &out);
	assert_bytes(out, len, (const unsigned char *)"\033[1;2A", 6);
	ASSERT(csiu_pending_size(&parser) == 0);

	make_long_text_sequence(long_text);
	len = translate(&parser, long_text, &out);
	assert_bytes(out, len, (const unsigned char *)long_text, strlen(long_text));

	return 0;
}
