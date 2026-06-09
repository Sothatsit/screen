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

#include "csiu.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum csiu_parse_result {
	CSIU_PARSE_INVALID,
	CSIU_PARSE_PARTIAL,
	CSIU_PARSE_DONE
};

static bool is_digit(unsigned char c)
{
	return c >= '0' && c <= '9';
}

static size_t utf8_size(uint32_t c)
{
	if (c < 0x80)
		return 1;
	if (c < 0x800)
		return 2;
	if (c < 0x10000)
		return 3;
	if (c <= 0x10ffff)
		return 4;
	return 0;
}

static void put_utf8(unsigned char *out, uint32_t c)
{
	if (c < 0x80) {
		out[0] = c;
		return;
	}
	if (c < 0x800) {
		out[0] = 0xc0 | (c >> 6);
		out[1] = 0x80 | (c & 0x3f);
		return;
	}
	if (c < 0x10000) {
		out[0] = 0xe0 | (c >> 12);
		out[1] = 0x80 | ((c >> 6) & 0x3f);
		out[2] = 0x80 | (c & 0x3f);
		return;
	}
	out[0] = 0xf0 | (c >> 18);
	out[1] = 0x80 | ((c >> 12) & 0x3f);
	out[2] = 0x80 | ((c >> 6) & 0x3f);
	out[3] = 0x80 | (c & 0x3f);
}

static bool append_byte(unsigned char *out, size_t out_cap, size_t *out_len,
			unsigned char c)
{
	if (*out_len >= out_cap)
		return false;
	out[(*out_len)++] = c;
	return true;
}

static bool append_utf8(unsigned char *out, size_t out_cap, size_t *out_len,
			uint32_t c)
{
	size_t len = utf8_size(c);

	if (!len)
		return true;
	if (len > out_cap - *out_len)
		return false;
	put_utf8(out + *out_len, c);
	*out_len += len;
	return true;
}

static enum csiu_parse_result parse_number(unsigned char **pp, unsigned char *end, uint32_t *value)
{
	unsigned char *p = *pp;
	uint32_t n = 0;

	if (p == end)
		return CSIU_PARSE_PARTIAL;
	if (!is_digit(*p))
		return CSIU_PARSE_INVALID;
	while (p < end && is_digit(*p)) {
		if (n <= 0x10ffff) {
			uint32_t digit = *p - '0';
			if (n <= (0x10ffff - digit) / 10)
				n = n * 10 + digit;
			else
				n = 0x110000;
		}
		p++;
	}
	*pp = p;
	*value = n;
	return CSIU_PARSE_DONE;
}

static enum csiu_parse_result parse_colon_number(unsigned char **pp, unsigned char *end, uint32_t *value)
{
	enum csiu_parse_result result;

	(*pp)++;
	result = parse_number(pp, end, value);
	if (result != CSIU_PARSE_DONE)
		return result;
	return CSIU_PARSE_DONE;
}

static bool write_text(unsigned char *out, size_t out_cap, size_t *out_len,
		       const uint32_t *text, size_t text_len)
{
	for (size_t i = 0; i < text_len; i++)
		if (!append_utf8(out, out_cap, out_len, text[i]))
			return false;
	return true;
}

static enum csiu_parse_result parse_csiu(unsigned char *start, unsigned char *end,
					 unsigned char **next, unsigned char *out,
					 size_t out_cap, size_t *out_len)
{
	unsigned char *p;
	uint32_t codepoint = 0;
	uint32_t ignored = 0;
	uint32_t modifiers_raw = 1;
	uint32_t event_type = 1;
	uint32_t text[CSIU_MAX_SEQUENCE / 2];
	size_t text_len = 0;
	enum csiu_parse_result result;

	*out_len = 0;

	if (end - start < 2)
		return CSIU_PARSE_PARTIAL;
	if (start[0] != '\033' || start[1] != '[')
		return CSIU_PARSE_INVALID;
	p = start + 2;

	result = parse_number(&p, end, &codepoint);
	if (result != CSIU_PARSE_DONE)
		return result;

	while (p < end && *p == ':') {
		result = parse_colon_number(&p, end, &ignored);
		if (result != CSIU_PARSE_DONE)
			return result;
	}

	if (p < end && *p == ';') {
		p++;
		if (p == end)
			return CSIU_PARSE_PARTIAL;
		if (is_digit(*p)) {
			result = parse_number(&p, end, &modifiers_raw);
			if (result != CSIU_PARSE_DONE)
				return result;
		}
		if (p < end && *p == ':') {
			result = parse_colon_number(&p, end, &event_type);
			if (result != CSIU_PARSE_DONE)
				return result;
		}
	}

	if (p < end && *p == ';') {
		do {
			p++;
			if (p == end)
				return CSIU_PARSE_PARTIAL;
			if (*p == 'u')
				break;
			result = parse_number(&p, end, &ignored);
			if (result != CSIU_PARSE_DONE)
				return result;
			if (text_len < sizeof(text) / sizeof(text[0]))
				text[text_len++] = ignored;
		} while (p < end && *p == ':');
	}

	if (p == end)
		return CSIU_PARSE_PARTIAL;
	if (*p != 'u')
		return CSIU_PARSE_INVALID;
	*next = p + 1;

	if (event_type == 3)
		return CSIU_PARSE_DONE;

	uint32_t modifiers = modifiers_raw > 0 ? modifiers_raw - 1 : 0;
	bool ctrl = (modifiers & 4) != 0;
	bool alt = (modifiers & 2) != 0;
	bool shift = (modifiers & 1) != 0;

	if (alt && !append_byte(out, out_cap, out_len, '\033'))
		return CSIU_PARSE_INVALID;

	if (ctrl && codepoint >= 64 && codepoint <= 127) {
		if (!append_byte(out, out_cap, out_len, codepoint & 0x1f))
			return CSIU_PARSE_INVALID;
	} else if (ctrl && codepoint == 32) {
		if (!append_byte(out, out_cap, out_len, 0))
			return CSIU_PARSE_INVALID;
	} else if (!ctrl && text_len > 0) {
		if (!write_text(out, out_cap, out_len, text, text_len))
			return CSIU_PARSE_INVALID;
	} else if (shift && codepoint >= 'a' && codepoint <= 'z') {
		if (!append_byte(out, out_cap, out_len, codepoint - 32))
			return CSIU_PARSE_INVALID;
	} else if (codepoint != 0) {
		if (!append_utf8(out, out_cap, out_len, codepoint))
			return CSIU_PARSE_INVALID;
	}
	return CSIU_PARSE_DONE;
}

void csiu_reset(CsiuParser *parser)
{
	parser->pending_len = 0;
}

size_t csiu_pending_size(const CsiuParser *parser)
{
	return parser->pending_len;
}

const unsigned char *csiu_pending_data(const CsiuParser *parser)
{
	return parser->pending;
}

size_t csiu_translate(CsiuParser *parser, unsigned char **bufp, size_t size)
{
	unsigned char *buf = *bufp;
	unsigned char *rp;
	unsigned char *wp;
	unsigned char *end;

	if (parser->pending_len) {
		buf -= parser->pending_len;
		memcpy(buf, parser->pending, parser->pending_len);
		size += parser->pending_len;
		parser->pending_len = 0;
		*bufp = buf;
	}

	rp = wp = buf;
	end = buf + size;
	while (rp < end) {
		if (*rp == '\033') {
			unsigned char translated[CSIU_MAX_SEQUENCE];
			unsigned char *next = NULL;
			size_t translated_len = 0;
			enum csiu_parse_result result;

			result = parse_csiu(rp, end, &next, translated,
					    sizeof(translated), &translated_len);
			if (result == CSIU_PARSE_DONE) {
				if (translated_len) {
					memmove(wp, translated, translated_len);
					wp += translated_len;
				}
				rp = next;
				continue;
			}
			if (result == CSIU_PARSE_PARTIAL) {
				size_t pending_len = end - rp;

				if (pending_len <= CSIU_MAX_SEQUENCE) {
					memcpy(parser->pending, rp, pending_len);
					parser->pending_len = pending_len;
					break;
				}
			}
		}
		*wp++ = *rp++;
	}

	return wp - buf;
}
