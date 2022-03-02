// Extract from basisu_transcoder.cpp
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

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

//************************** Helpers and Boilerplate **************************/

enum {
	cETC1IntenModifierNumBits = 3,
	cETC1IntenModifierValues = 1 << cETC1IntenModifierNumBits,
	cETC1SelectorBits = 2U,
	cETC1SelectorValues = 1U << cETC1SelectorBits,
};

enum eNoClamp { cNoClamp };

static int clamp255(int x)
{
	if (x & 0xFFFFFF00)
	{
		if (x < 0)
			x = 0;
		else if (x > 255)
			x = 255;
	}

	return x;
}

#define DECLARE_ETC1_INTEN_TABLE(name, N) \
static const int name[cETC1IntenModifierValues][cETC1SelectorValues] = \
{ \
	{ N * -8,  N * -2,	 N * 2,	  N * 8 },{ N * -17,  N * -5,  N * 5,  N * 17 },{ N * -29,	N * -9,	  N * 9,  N * 29 },{ N * -42, N * -13, N * 13,	N * 42 }, \
	{ N * -60, N * -18, N * 18,	 N * 60 },{ N * -80, N * -24, N * 24,  N * 80 },{ N * -106, N * -33, N * 33, N * 106 },{ N * -183, N * -47, N * 47, N * 183 } \
};

DECLARE_ETC1_INTEN_TABLE(g_etc1_inten_tables, 1);

namespace basisu {
	template <typename S> inline S clamp(S value, S low, S high) { return (value < low) ? low : ((value > high) ? high : value); }
	template <typename S> inline S minimum(S a, S b) {	return (a < b) ? a : b; }
	template <typename S> inline S maximum(S a, S b) {	return (a > b) ? a : b; }
};

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
};

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

/**
 * Helper to return the current time in milliseconds.
 */
static unsigned millis() {
	return static_cast<unsigned>((clock() * 1000LL) / CLOCKS_PER_SEC);
}

/**
 * Prebuilt table with known results.
 */
static const etc1_to_dxt1_56_solution known[32 * 8 * NUM_ETC1_TO_DXT1_SELECTOR_MAPPINGS * NUM_ETC1_TO_DXT1_SELECTOR_RANGES] = {
#include "basisu_transcoder_tables_dxt1_6.inc"
};

//************************ Optimisation Task Goes Here ************************/

/**
 * Results stored here.
 */
static etc1_to_dxt1_56_solution result[32 * 8 * NUM_ETC1_TO_DXT1_SELECTOR_MAPPINGS * NUM_ETC1_TO_DXT1_SELECTOR_RANGES];

/**
 * Function to optimise.
 */
static void create_etc1_to_dxt1_6_conversion_table()
{
	uint32_t n = 0;

	for (int inten = 0; inten < 8; inten++)
	{
		for (uint32_t g = 0; g < 32; g++)
		{
			color32 block_colors[4];
			decoder_etc_block::get_diff_subblock_colors(block_colors, decoder_etc_block::pack_color5(color32(g, g, g, 255), false), inten);

			for (uint32_t sr = 0; sr < NUM_ETC1_TO_DXT1_SELECTOR_RANGES; sr++)
			{
				const uint32_t low_selector = g_etc1_to_dxt1_selector_ranges[sr].m_low;
				const uint32_t high_selector = g_etc1_to_dxt1_selector_ranges[sr].m_high;

				for (uint32_t m = 0; m < NUM_ETC1_TO_DXT1_SELECTOR_MAPPINGS; m++)
				{
					uint32_t best_lo = 0;
					uint32_t best_hi = 0;
					uint32_t best_err = UINT32_MAX;

					for (uint32_t hi = 0; hi <= 63; hi++)
					{
						for (uint32_t lo = 0; lo <= 63; lo++)
						{
							uint32_t colors[4];

							colors[0] = (lo << 2) | (lo >> 4);
							colors[3] = (hi << 2) | (hi >> 4);

							colors[1] = (colors[0] * 2 + colors[3]) / 3;
							colors[2] = (colors[3] * 2 + colors[0]) / 3;

							uint32_t total_err = 0;

							for (uint32_t s = low_selector; s <= high_selector; s++)
							{
								int err = block_colors[s].g - colors[g_etc1_to_dxt1_selector_mappings[m][s]];

								total_err += err * err;
							}

							if (total_err < best_err)
							{
								best_err = total_err;
								best_lo = lo;
								best_hi = hi;
							}
						}
					}

					assert(best_err <= 0xFFFF);

					result[n] = (etc1_to_dxt1_56_solution){ (uint8_t)best_lo, (uint8_t)best_hi, (uint16_t)best_err };

					n++;
				} // m
			} // sr
		} // g
	} // inten
}

/**
 * Tests the generation and benchmarks it.
 */
int main(int /*argc*/, char* /*argv*/[]) {
	// Run this once and compare the result to the known table
	create_etc1_to_dxt1_6_conversion_table();
    assert(memcmp(result, known, sizeof(known)) == 0);
    
    // Perform multiple runs and take the best time
    unsigned best = UINT32_MAX;
    for (int n = 10; n > 0; n--) {
    	unsigned time = millis();
    	create_etc1_to_dxt1_6_conversion_table();
    	time = millis() - time;
    	if (time < best) {
    		best = time;
    	}
    }
    
    printf("Best run took %dms\n", best);
    return 0;
}
