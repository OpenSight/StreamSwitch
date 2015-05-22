#line 1 "network/ragel_range.rl"
/* -*- c -*- */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "network/rtsp.h"
#include "network/ragel_parsers.h"

#line 11 "network/ragel_range.rl"

gboolean ragel_parse_range_header(const char *header,
                                  RTSP_Range *range) {

    int cs;
    const char *p = header, *pe = p + strlen(p) +1;

    gboolean range_supported = false;

    /* Total seconds expressed */
    double seconds = 0;

    /* Integer part of seconds, minutes and hours */
    double integer = 0;

    /* fractional part of seconds, denominator and numerator */
    double seconds_secfrac_num = 0;
    double seconds_secfrac_den = 0;

    struct tm utctime;

    
#line 35 "network/ragel_range.c"
static const char _ragel_range_header_actions[] = {
	0, 1, 1, 1, 2, 1, 3, 1, 
	4, 1, 6, 1, 11, 1, 12, 1, 
	13, 1, 14, 1, 15, 1, 16, 1, 
	17, 1, 18, 1, 19, 2, 0, 1, 
	2, 4, 9, 2, 5, 6, 2, 7, 
	9, 2, 8, 9, 3, 4, 10, 11, 
	3, 7, 10, 11, 3, 8, 10, 11
	
};

static const short _ragel_range_header_key_offsets[] = {
	0, 0, 5, 10, 12, 17, 23, 29, 
	35, 41, 51, 60, 79, 88, 105, 116, 
	126, 132, 143, 161, 175, 182, 188, 201, 
	213, 230, 239, 249, 260, 270, 276, 287, 
	298, 305, 312, 318, 324, 330, 336, 342, 
	348, 357, 366, 375, 384, 391, 400, 410, 
	419, 425, 435, 444, 453, 462, 471, 480, 
	487, 496, 506, 519, 528, 537, 546, 552, 
	558, 563, 571, 577
};

static const char _ragel_range_header_trans_keys[] = {
	110, 65, 90, 97, 122, 61, 65, 90, 
	97, 122, 32, 126, 0, 59, 110, 32, 
	126, 0, 59, 110, 116, 32, 126, 0, 
	59, 110, 112, 32, 126, 0, 59, 110, 
	116, 32, 126, 0, 59, 61, 110, 32, 
	126, 0, 45, 59, 110, 32, 47, 48, 
	57, 58, 126, 0, 59, 110, 32, 47, 
	48, 57, 58, 126, 0, 46, 58, 59, 
	110, 32, 47, 48, 57, 60, 64, 65, 
	90, 91, 96, 97, 122, 123, 126, 0, 
	59, 110, 32, 47, 48, 57, 58, 126, 
	0, 59, 110, 32, 47, 48, 57, 58, 
	64, 65, 90, 91, 96, 97, 122, 123, 
	126, 0, 59, 110, 32, 47, 48, 53, 
	54, 57, 58, 126, 0, 58, 59, 110, 
	32, 47, 48, 57, 60, 126, 0, 58, 
	59, 110, 32, 126, 0, 59, 110, 32, 
	47, 48, 53, 54, 57, 58, 126, 0, 
	46, 59, 110, 32, 47, 48, 57, 58, 
	64, 65, 90, 91, 96, 97, 122, 123, 
	126, 0, 46, 59, 110, 32, 64, 65, 
	90, 91, 96, 97, 122, 123, 126, 0, 
	59, 110, 111, 112, 32, 126, 0, 59, 
	110, 119, 32, 126, 0, 59, 110, 32, 
	64, 65, 90, 91, 96, 97, 122, 123, 
	126, 0, 45, 46, 58, 59, 110, 32, 
	47, 48, 57, 60, 126, 0, 59, 110, 
	32, 47, 48, 57, 58, 64, 65, 90, 
	91, 96, 97, 122, 123, 126, 0, 59, 
	110, 32, 47, 48, 57, 58, 126, 0, 
	45, 59, 110, 32, 47, 48, 57, 58, 
	126, 0, 59, 110, 32, 47, 48, 53, 
	54, 57, 58, 126, 0, 58, 59, 110, 
	32, 47, 48, 57, 60, 126, 0, 58, 
	59, 110, 32, 126, 0, 59, 110, 32, 
	47, 48, 53, 54, 57, 58, 126, 0, 
	45, 46, 59, 110, 32, 47, 48, 57, 
	58, 126, 0, 45, 46, 59, 110, 32, 
	126, 0, 59, 110, 111, 112, 32, 126, 
	0, 59, 110, 119, 32, 126, 0, 45, 
	59, 110, 32, 126, 0, 59, 105, 110, 
	32, 126, 0, 59, 109, 110, 32, 126, 
	0, 59, 101, 110, 32, 126, 0, 59, 
	61, 110, 32, 126, 0, 59, 110, 32, 
	47, 48, 57, 58, 126, 0, 59, 110, 
	32, 47, 48, 57, 58, 126, 0, 59, 
	110, 32, 47, 48, 57, 58, 126, 0, 
	59, 110, 32, 47, 48, 57, 58, 126, 
	0, 48, 49, 59, 110, 32, 126, 0, 
	59, 110, 32, 47, 48, 57, 58, 126, 
	0, 51, 59, 110, 32, 47, 48, 50, 
	52, 126, 0, 59, 110, 32, 47, 48, 
	57, 58, 126, 0, 59, 84, 110, 32, 
	126, 0, 50, 59, 110, 32, 47, 48, 
	49, 51, 126, 0, 59, 110, 32, 47, 
	48, 57, 58, 126, 0, 59, 110, 32, 
	47, 48, 53, 54, 126, 0, 59, 110, 
	32, 47, 48, 57, 58, 126, 0, 59, 
	110, 32, 47, 48, 53, 54, 126, 0, 
	59, 110, 32, 47, 48, 57, 58, 126, 
	0, 46, 59, 90, 110, 32, 126, 0, 
	59, 110, 32, 47, 48, 57, 58, 126, 
	0, 59, 90, 110, 32, 47, 48, 57, 
	58, 126, 0, 59, 110, 32, 64, 65, 
	90, 91, 96, 97, 122, 123, 126, 0, 
	59, 110, 32, 47, 48, 51, 52, 126, 
	0, 59, 110, 32, 47, 48, 49, 50, 
	126, 0, 59, 110, 32, 48, 49, 50, 
	51, 126, 61, 112, 65, 90, 97, 122, 
	61, 116, 65, 90, 97, 122, 61, 65, 
	90, 97, 122, 45, 110, 32, 47, 48, 
	57, 58, 126, 0, 59, 110, 111, 32, 
	126, 0
};

static const char _ragel_range_header_single_lengths[] = {
	0, 1, 1, 0, 3, 4, 4, 4, 
	4, 4, 3, 5, 3, 3, 3, 4, 
	4, 3, 4, 4, 5, 4, 3, 6, 
	3, 3, 4, 3, 4, 4, 3, 5, 
	5, 5, 4, 4, 4, 4, 4, 4, 
	3, 3, 3, 3, 5, 3, 4, 3, 
	4, 4, 3, 3, 3, 3, 3, 5, 
	3, 4, 3, 3, 3, 3, 2, 2, 
	1, 2, 4, 0
};

static const char _ragel_range_header_range_lengths[] = {
	0, 2, 2, 1, 1, 1, 1, 1, 
	1, 3, 3, 7, 3, 7, 4, 3, 
	1, 4, 7, 5, 1, 1, 5, 3, 
	7, 3, 3, 4, 3, 1, 4, 3, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	3, 3, 3, 3, 1, 3, 3, 3, 
	1, 3, 3, 3, 3, 3, 3, 1, 
	3, 3, 5, 3, 3, 3, 2, 2, 
	2, 3, 1, 0
};

static const short _ragel_range_header_index_offsets[] = {
	0, 0, 4, 8, 10, 15, 21, 27, 
	33, 39, 47, 54, 67, 74, 85, 93, 
	101, 107, 115, 127, 137, 144, 150, 159, 
	169, 180, 187, 195, 203, 211, 217, 225, 
	234, 241, 248, 254, 260, 266, 272, 278, 
	284, 291, 298, 305, 312, 319, 326, 334, 
	341, 347, 355, 362, 369, 376, 383, 390, 
	397, 404, 412, 421, 428, 435, 442, 447, 
	452, 456, 462, 468
};

static const char _ragel_range_header_indicies[] = {
	2, 0, 0, 1, 3, 0, 0, 1, 
	4, 1, 5, 6, 7, 4, 1, 5, 
	6, 7, 8, 4, 1, 5, 6, 7, 
	9, 4, 1, 5, 6, 7, 10, 4, 
	1, 5, 6, 11, 7, 4, 1, 5, 
	12, 6, 14, 4, 13, 4, 1, 5, 
	6, 16, 4, 15, 4, 1, 17, 18, 
	20, 21, 23, 4, 19, 4, 22, 4, 
	22, 4, 1, 5, 6, 7, 4, 24, 
	4, 1, 25, 27, 29, 4, 26, 4, 
	28, 4, 28, 4, 1, 5, 6, 7, 
	4, 30, 31, 4, 1, 5, 33, 6, 
	7, 4, 32, 4, 1, 5, 33, 6, 
	7, 4, 1, 5, 6, 7, 4, 34, 
	35, 4, 1, 17, 18, 21, 23, 4, 
	36, 4, 22, 4, 22, 4, 1, 17, 
	18, 21, 23, 4, 22, 4, 22, 4, 
	1, 5, 6, 7, 37, 9, 4, 1, 
	5, 6, 7, 38, 4, 1, 39, 40, 
	42, 4, 41, 4, 41, 4, 1, 5, 
	43, 44, 46, 6, 7, 4, 45, 4, 
	1, 47, 48, 50, 4, 15, 4, 49, 
	4, 49, 4, 1, 5, 6, 7, 4, 
	51, 4, 1, 5, 52, 6, 7, 4, 
	53, 4, 1, 5, 6, 7, 4, 54, 
	55, 4, 1, 5, 57, 6, 7, 4, 
	56, 4, 1, 5, 57, 6, 7, 4, 
	1, 5, 6, 7, 4, 58, 59, 4, 
	1, 5, 43, 44, 6, 7, 4, 60, 
	4, 1, 5, 43, 44, 6, 7, 4, 
	1, 5, 6, 7, 61, 9, 4, 1, 
	5, 6, 7, 62, 4, 1, 5, 63, 
	6, 7, 4, 1, 5, 6, 64, 7, 
	4, 1, 5, 6, 65, 7, 4, 1, 
	5, 6, 66, 7, 4, 1, 5, 6, 
	67, 7, 4, 1, 5, 6, 7, 4, 
	68, 4, 1, 5, 6, 7, 4, 69, 
	4, 1, 5, 6, 7, 4, 70, 4, 
	1, 5, 6, 7, 4, 71, 4, 1, 
	5, 72, 73, 6, 7, 4, 1, 5, 
	6, 7, 4, 74, 4, 1, 5, 76, 
	6, 7, 4, 75, 4, 1, 5, 6, 
	7, 4, 77, 4, 1, 5, 6, 78, 
	7, 4, 1, 5, 80, 6, 7, 4, 
	79, 4, 1, 5, 6, 7, 4, 81, 
	4, 1, 5, 6, 7, 4, 82, 4, 
	1, 5, 6, 7, 4, 83, 4, 1, 
	5, 6, 7, 4, 84, 4, 1, 5, 
	6, 7, 4, 85, 4, 1, 5, 86, 
	6, 87, 7, 4, 1, 5, 6, 7, 
	4, 88, 4, 1, 5, 6, 87, 7, 
	4, 88, 4, 1, 89, 6, 91, 4, 
	90, 4, 90, 4, 1, 5, 6, 7, 
	4, 81, 4, 1, 5, 6, 7, 4, 
	77, 4, 1, 5, 6, 7, 4, 74, 
	4, 1, 3, 92, 0, 0, 1, 3, 
	93, 0, 0, 1, 94, 0, 0, 1, 
	12, 95, 4, 13, 4, 1, 5, 6, 
	7, 61, 4, 1, 1, 0
};

static const char _ragel_range_header_trans_targs[] = {
	2, 0, 62, 3, 4, 67, 5, 6, 
	36, 7, 8, 9, 10, 23, 33, 11, 
	20, 67, 12, 11, 14, 5, 4, 6, 
	13, 67, 13, 5, 4, 6, 15, 16, 
	16, 17, 18, 19, 19, 21, 22, 67, 
	5, 4, 6, 24, 25, 23, 27, 67, 
	5, 4, 20, 26, 24, 26, 28, 29, 
	29, 30, 31, 32, 32, 34, 35, 24, 
	37, 38, 39, 40, 41, 42, 43, 44, 
	45, 61, 46, 47, 60, 48, 49, 50, 
	59, 51, 52, 53, 54, 55, 56, 58, 
	57, 67, 4, 6, 63, 64, 65, 66
};

static const char _ragel_range_header_trans_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 29, 0, 29, 
	0, 44, 7, 1, 3, 44, 44, 44, 
	35, 48, 9, 48, 48, 48, 29, 29, 
	1, 5, 29, 29, 1, 0, 0, 52, 
	52, 52, 52, 32, 7, 1, 3, 11, 
	11, 11, 11, 35, 38, 9, 29, 29, 
	1, 5, 29, 29, 1, 0, 0, 41, 
	0, 0, 0, 0, 13, 0, 0, 15, 
	0, 0, 17, 0, 0, 19, 0, 0, 
	0, 21, 0, 23, 0, 25, 0, 0, 
	0, 27, 27, 27, 0, 0, 0, 0
};

static const int ragel_range_header_start = 1;

static const int ragel_range_header_en_main = 1;


#line 268 "network/ragel_range.c"
	{
	cs = ragel_range_header_start;
	}

#line 273 "network/ragel_range.c"
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
	_keys = _ragel_range_header_trans_keys + _ragel_range_header_key_offsets[cs];
	_trans = _ragel_range_header_index_offsets[cs];

	_klen = _ragel_range_header_single_lengths[cs];
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

	_klen = _ragel_range_header_range_lengths[cs];
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
	_trans = _ragel_range_header_indicies[_trans];
	cs = _ragel_range_header_trans_targs[_trans];

	if ( _ragel_range_header_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _ragel_range_header_actions + _ragel_range_header_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 34 "network/ragel_range.rl"
	{
            integer = 0;
        }
	break;
	case 1:
#line 38 "network/ragel_range.rl"
	{
            integer *= 10;
            integer += (*p) - '0';
        }
	break;
	case 2:
#line 43 "network/ragel_range.rl"
	{
            seconds += integer * 3600;
        }
	break;
	case 3:
#line 47 "network/ragel_range.rl"
	{
            seconds += integer * 60;
        }
	break;
	case 4:
#line 51 "network/ragel_range.rl"
	{
            seconds += integer * 1;
        }
	break;
	case 5:
#line 55 "network/ragel_range.rl"
	{
            seconds_secfrac_num = 0;
            seconds_secfrac_den = 1;
        }
	break;
	case 6:
#line 60 "network/ragel_range.rl"
	{
            seconds_secfrac_num *= 10;
            seconds_secfrac_num += (*p) - '0';
            seconds_secfrac_den *= 10;
        }
	break;
	case 7:
#line 66 "network/ragel_range.rl"
	{
            seconds += seconds_secfrac_num / seconds_secfrac_den;
        }
	break;
	case 8:
#line 80 "network/ragel_range.rl"
	{ return false; }
	break;
	case 9:
#line 83 "network/ragel_range.rl"
	{
            range->begin_time = seconds;
            seconds = 0;
        }
	break;
	case 10:
#line 88 "network/ragel_range.rl"
	{
            range->end_time = seconds;
            seconds = 0;
        }
	break;
	case 11:
#line 96 "network/ragel_range.rl"
	{ range_supported = true; }
	break;
	case 12:
#line 100 "network/ragel_range.rl"
	{
            memset(&utctime, 0, sizeof(struct tm));
        }
	break;
	case 13:
#line 104 "network/ragel_range.rl"
	{
            utctime.tm_year *= 10;
            utctime.tm_year += (*p) - '0';
        }
	break;
	case 14:
#line 109 "network/ragel_range.rl"
	{
            utctime.tm_mon *= 10;
            utctime.tm_mon += (*p) - '0';
        }
	break;
	case 15:
#line 114 "network/ragel_range.rl"
	{
            utctime.tm_mday *= 10;
            utctime.tm_mday += (*p) - '0';
        }
	break;
	case 16:
#line 124 "network/ragel_range.rl"
	{
            utctime.tm_hour *= 10;
            utctime.tm_hour += (*p) - '0';
        }
	break;
	case 17:
#line 129 "network/ragel_range.rl"
	{
            utctime.tm_min *= 10;
            utctime.tm_min += (*p) - '0';
        }
	break;
	case 18:
#line 134 "network/ragel_range.rl"
	{
            utctime.tm_sec *= 10;
            utctime.tm_sec += (*p) - '0';
        }
	break;
	case 19:
#line 147 "network/ragel_range.rl"
	{
            range->playback_time = mktime(&utctime);
        }
	break;
#line 475 "network/ragel_range.c"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	_out: {}
	}
#line 160 "network/ragel_range.rl"


    cs = ragel_range_header_en_main;

    return range_supported;
}
