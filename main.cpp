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

#include "basisu_headers.h"

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

//******************************** Entry Point ********************************/

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
