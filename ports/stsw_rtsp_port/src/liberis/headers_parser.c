#line 1 "liberis/headers_parser.rl"
/* -*-c-*- */
/* This file is part of liberis
 *
 * Copyright (C) 2008 by LScube team <team@streaming.polito.it>
 * See AUTHORS for more details
 *
 * liberis is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * liberis is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with liberis.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "liberis/headers.h"
#include "liberis/utils.h"

#line 62 "liberis/headers_parser.rl"


/**
 * @brief Parse a string containing RTSP headers in a GHashTable
 * @param hdrs_string The string containing the series of headers
 * @param len Length of the parameters string, included final newline.
 * @param table Pointer where to write the actual GHashTable instance
 *              pointer containing the parsed headers.
 *
 * @return The number of characters parsed from the input
 *
 * This function can be used to parse RTSP request, response and
 * entity headers into an hash table that can then be used to access
 * them.
 *
 * The reason why this function takes a pointer and a size is that it
 * can be used directly from a different Ragel-based parser, like the
 * request or response parser, where the string is not going to be
 * NULL-terminated.
 *
 * Whenever the returned size is zero, the parser encountered a
 * problem and couldn't complete the parsing, the @p table parameter
 * is undefined.
 */
size_t eris_parse_headers(const char *hdrs_string, size_t len, GHashTable **table)
{
  const char *p = hdrs_string, *pe = p + len, *eof = pe;

  int cs, line;

  const char *hdr = NULL, *hdr_val = NULL;
  size_t hdr_size = 0, hdr_val_size = 0;

  /* Create the new hash table */
  *table = _eris_hdr_table_new();

  
#line 67 "liberis/headers_parser.c"
static const char _headers_parser_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 2, 0, 1, 
	2, 0, 6, 2, 4, 5
};

static const char _headers_parser_key_offsets[] = {
	0, 0, 10, 21, 22, 24, 28, 40, 
	42, 44, 44
};

static const char _headers_parser_trans_keys[] = {
	95, 126, 45, 46, 48, 57, 65, 90, 
	97, 122, 58, 95, 126, 45, 46, 48, 
	57, 65, 90, 97, 122, 32, 32, 126, 
	10, 13, 32, 126, 10, 13, 95, 126, 
	45, 46, 48, 57, 65, 90, 97, 122, 
	10, 13, 10, 13, 0
};

static const char _headers_parser_single_lengths[] = {
	0, 2, 3, 1, 0, 2, 4, 2, 
	2, 0, 0
};

static const char _headers_parser_range_lengths[] = {
	0, 4, 4, 0, 1, 1, 4, 0, 
	0, 0, 0
};

static const char _headers_parser_index_offsets[] = {
	0, 0, 7, 15, 17, 19, 23, 32, 
	35, 38, 39
};

static const char _headers_parser_indicies[] = {
	0, 0, 0, 0, 0, 0, 1, 3, 
	2, 2, 2, 2, 2, 2, 1, 4, 
	1, 5, 1, 6, 7, 8, 1, 9, 
	10, 11, 11, 11, 11, 11, 11, 1, 
	12, 13, 1, 14, 15, 1, 16, 17, 
	0
};

static const char _headers_parser_trans_targs[] = {
	2, 0, 2, 3, 4, 5, 6, 8, 
	5, 9, 7, 2, 9, 7, 6, 8, 
	10, 10
};

static const char _headers_parser_trans_actions[] = {
	3, 0, 0, 5, 0, 7, 19, 9, 
	0, 1, 1, 13, 0, 0, 11, 0, 
	16, 0
};

static const char _headers_parser_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 16, 0
};

static const int headers_parser_start = 1;
static const int headers_parser_first_final = 9;

static const int headers_parser_en_main = 1;

#line 99 "liberis/headers_parser.rl"
  
#line 136 "liberis/headers_parser.c"
	{
	cs = headers_parser_start;
	}
#line 100 "liberis/headers_parser.rl"
  
#line 142 "liberis/headers_parser.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _headers_parser_trans_keys + _headers_parser_key_offsets[cs];
	_trans = _headers_parser_index_offsets[cs];

	_klen = _headers_parser_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _headers_parser_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += ((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _headers_parser_indicies[_trans];
	cs = _headers_parser_trans_targs[_trans];

	if ( _headers_parser_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _headers_parser_actions + _headers_parser_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 9 "liberis/headers_parser.rl"
	{line++;}
	break;
	case 1:
#line 31 "liberis/headers_parser.rl"
	{
    hdr = p;
  }
	break;
	case 2:
#line 35 "liberis/headers_parser.rl"
	{
    hdr_size = p-hdr;
  }
	break;
	case 3:
#line 39 "liberis/headers_parser.rl"
	{
    hdr_val = p;
  }
	break;
	case 4:
#line 43 "liberis/headers_parser.rl"
	{
    hdr_val_size = p-hdr_val;
  }
	break;
	case 5:
#line 50 "liberis/headers_parser.rl"
	{
      g_hash_table_insert(*table,
                          g_strndup(hdr, hdr_size),
                          g_strndup(hdr_val, hdr_val_size));
  }
	break;
	case 6:
#line 56 "liberis/headers_parser.rl"
	{
      return p-hdrs_string;
  }
	break;
#line 258 "liberis/headers_parser.c"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _headers_parser_actions + _headers_parser_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 0:
#line 9 "liberis/headers_parser.rl"
	{line++;}
	break;
	case 6:
#line 56 "liberis/headers_parser.rl"
	{
      return p-hdrs_string;
  }
	break;
#line 284 "liberis/headers_parser.c"
		}
	}
	}

	_out: {}
	}
#line 101 "liberis/headers_parser.rl"

  if ( cs < headers_parser_first_final ) {
    g_hash_table_destroy(*table);
    return 0;
  }

  cs = headers_parser_en_main;

  return p-hdrs_string;
}
