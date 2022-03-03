#pragma once

// Extracts from basisu.h, basisu_etc.h and basisu_transcoder_internal.h
// Copyright (C) 2019-2021 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

enum {
	cETC1IntenModifierNumBits = 3,
	cETC1IntenModifierValues = 1 << cETC1IntenModifierNumBits,
	cETC1SelectorBits = 2U,
	cETC1SelectorValues = 1U << cETC1SelectorBits
};

enum eNoClamp { cNoClamp };

static uint8_t clamp255(int32_t i)
{
	return (uint8_t)((i & 0xFFFFFF00U) ? (~(i >> 31)) : i);
}

#define DECLARE_ETC1_INTEN_TABLE(name, N) \
static const int name[cETC1IntenModifierValues][cETC1SelectorValues] = \
{ \
	{ N * -8,  N * -2,	 N * 2,	  N * 8 },{ N * -17,  N * -5,  N * 5,  N * 17 },{ N * -29,	N * -9,	  N * 9,  N * 29 },{ N * -42, N * -13, N * 13,	N * 42 }, \
	{ N * -60, N * -18, N * 18,	 N * 60 },{ N * -80, N * -24, N * 24,  N * 80 },{ N * -106, N * -33, N * 33, N * 106 },{ N * -183, N * -47, N * 47, N * 183 } \
};

DECLARE_ETC1_INTEN_TABLE(g_etc1_inten_tables, 1)

namespace basisu {
	template <typename S> inline S clamp(S value, S low, S high) { return (value < low) ? low : ((value > high) ? high : value); }
	template <typename S> inline S minimum(S a, S b) {	return (a < b) ? a : b; }
	template <typename S> inline S maximum(S a, S b) {	return (a > b) ? a : b; }
}

struct color32
{
	union
	{
		struct
		{
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		};

		uint8_t c[4];

		uint32_t m;
	};

	color32() { }

	color32(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { set(vr, vg, vb, va); }
	color32(eNoClamp unused, uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { (void)unused; set_noclamp_rgba(vr, vg, vb, va); }

	void set(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { c[0] = static_cast<uint8_t>(vr); c[1] = static_cast<uint8_t>(vg); c[2] = static_cast<uint8_t>(vb); c[3] = static_cast<uint8_t>(va); }

	void set_noclamp_rgb(uint32_t vr, uint32_t vg, uint32_t vb) { c[0] = static_cast<uint8_t>(vr); c[1] = static_cast<uint8_t>(vg); c[2] = static_cast<uint8_t>(vb); }
	void set_noclamp_rgba(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { set(vr, vg, vb, va); }

	void set_clamped(int vr, int vg, int vb, int va) { c[0] = clamp255(vr); c[1] = clamp255(vg);	c[2] = clamp255(vb); c[3] = clamp255(va); }

	uint8_t operator[] (uint32_t idx) const { assert(idx < 4); return c[idx]; }
	uint8_t &operator[] (uint32_t idx) { assert(idx < 4); return c[idx]; }

	bool operator== (const color32&rhs) const { return m == rhs.m; }

	static color32 comp_min(const color32& a, const color32& b) { return color32(cNoClamp, basisu::minimum(a[0], b[0]), basisu::minimum(a[1], b[1]), basisu::minimum(a[2], b[2]), basisu::minimum(a[3], b[3])); }
	static color32 comp_max(const color32& a, const color32& b) { return color32(cNoClamp, basisu::maximum(a[0], b[0]), basisu::maximum(a[1], b[1]), basisu::maximum(a[2], b[2]), basisu::maximum(a[3], b[3])); }
};

namespace decoder_etc_block {

	static uint16_t pack_color5(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias = 127U)
	{
		if (scaled)
		{
			r = (r * 31U + bias) / 255U;
			g = (g * 31U + bias) / 255U;
			b = (b * 31U + bias) / 255U;
		}

		r = basisu::minimum(r, 31U);
		g = basisu::minimum(g, 31U);
		b = basisu::minimum(b, 31U);

		return static_cast<uint16_t>(b | (g << 5U) | (r << 10U));
	}

	static uint16_t pack_color5(const color32& color, bool scaled, uint32_t bias = 127U)
	{
		return pack_color5(color.r, color.g, color.b, scaled, bias);
	}

	static color32 unpack_color5(uint16_t packed_color5, bool scaled, uint32_t alpha)
	{
		uint32_t b = packed_color5 & 31U;
		uint32_t g = (packed_color5 >> 5U) & 31U;
		uint32_t r = (packed_color5 >> 10U) & 31U;

		if (scaled)
		{
			b = (b << 3U) | (b >> 2U);
			g = (g << 3U) | (g >> 2U);
			r = (r << 3U) | (r >> 2U);
		}

		assert(alpha <= 255);

		return color32(cNoClamp, r, g, b, alpha);
	}

	static void unpack_color5(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color5, bool scaled)
	{
		color32 c(unpack_color5(packed_color5, scaled, 0));
		r = c.r;
		g = c.g;
		b = c.b;
	}

	static void get_diff_subblock_colors(color32* pDst, uint16_t packed_color5, uint32_t table_idx)
	{
		assert(table_idx < cETC1IntenModifierValues);
		const int* pInten_modifer_table = &g_etc1_inten_tables[table_idx][0];

		uint32_t r, g, b;
		unpack_color5(r, g, b, packed_color5, true);

		const int ir = static_cast<int>(r), ig = static_cast<int>(g), ib = static_cast<int>(b);

		const int y0 = pInten_modifer_table[0];
		pDst[0].set(clamp255(ir + y0), clamp255(ig + y0), clamp255(ib + y0), 255);

		const int y1 = pInten_modifer_table[1];
		pDst[1].set(clamp255(ir + y1), clamp255(ig + y1), clamp255(ib + y1), 255);

		const int y2 = pInten_modifer_table[2];
		pDst[2].set(clamp255(ir + y2), clamp255(ig + y2), clamp255(ib + y2), 255);

		const int y3 = pInten_modifer_table[3];
		pDst[3].set(clamp255(ir + y3), clamp255(ig + y3), clamp255(ib + y3), 255);
	}
}

struct dxt_selector_range
{
	uint32_t m_low;
	uint32_t m_high;
};

static dxt_selector_range g_etc1_to_dxt1_selector_ranges[] =
{
	{ 0, 3 },

	{ 1, 3 },
	{ 0, 2 },

	{ 1, 2 },

	{ 2, 3 },
	{ 0, 1 },
};

const uint32_t NUM_ETC1_TO_DXT1_SELECTOR_RANGES = sizeof(g_etc1_to_dxt1_selector_ranges) / sizeof(g_etc1_to_dxt1_selector_ranges[0]);

const uint32_t NUM_ETC1_TO_DXT1_SELECTOR_MAPPINGS = 10;
static const uint8_t g_etc1_to_dxt1_selector_mappings[NUM_ETC1_TO_DXT1_SELECTOR_MAPPINGS][4] =
{
	{ 0, 0, 1, 1 },
	{ 0, 0, 1, 2 },
	{ 0, 0, 1, 3 },
	{ 0, 0, 2, 3 },
	{ 0, 1, 1, 1 },
	{ 0, 1, 2, 2 },
	{ 0, 1, 2, 3 },
	{ 0, 2, 3, 3 },
	{ 1, 2, 2, 2 },
	{ 1, 2, 3, 3 },
};

struct etc1_to_dxt1_56_solution
{
	uint8_t m_lo;
	uint8_t m_hi;
	uint16_t m_err;
};
