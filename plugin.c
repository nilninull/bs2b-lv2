/*
 *  LV2 bs2b effect plugin 
 * 
 * 
 * Copyright (C) 2016, 2021  nilninull
 *
 * Author: nilninull <nilninull@gmail.com>
 *
 *
 * This program was ported from LADSPA bs2b effect plugin.
 * <http://bs2b.sourceforge.net/>
 * Original author is Sebastian Pipping (C) 2009 <sebastian@pipping.org>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Which is a better place for lv2 uri? */
#define BS2B_URI "http://bs2b.sourceforge.net/plugins/bs2b"
/* #define BS2B_URI "https://github.com/nilninull/bs2b-lv2/plugins/bs2b" */

#include <lv2.h>
#include <bs2b.h>

#include <stdlib.h>

#define LB_BETWEEN(min, x, max) (((x) < (min))\
	? (min)\
	: (\
		((x) > (max))\
		? (max)\
		: (x)\
	)\
)

/*****************************************************************************/

/* The port numbers for the plugin: */
typedef enum {
	LB_PORT_LOWPASS,
	LB_PORT_FEEDING,
	LB_PORT_INPUT_LEFT,
	LB_PORT_INPUT_RIGHT,
	LB_PORT_OUTPUT_LEFT,
	LB_PORT_OUTPUT_RIGHT,
} PortIndex;

/*****************************************************************************/

/* Instance data for the bs2b plugin. */
typedef struct {
	t_bs2bdp bs2b;
	uint32_t levelBackup;

	float * alternatingBuffer;
	size_t bufferSampleCount;

	/* Ports from here on */
	float * m_pfLowpass;
	float * m_pfFeeding;

	/* Input audio port data location. */
	float * m_pfInputLeft;
	float * m_pfInputRight;

	/* Output audio port data location. */
	float * m_pfOutputLeft;
	float * m_pfOutputRight;
} Bs2bLine;

/*****************************************************************************/

/* Construct a new plugin instance. */
static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features) {
	/* Sample rate supported? */
	if ((rate < BS2B_MINSRATE) || (rate > BS2B_MAXSRATE)) {
		return NULL;
	}

	Bs2bLine * psBs2bLine = (Bs2bLine *)malloc(sizeof(Bs2bLine));
	if (psBs2bLine == NULL) {
		return NULL;
	}

	/* Init effect backend */
	psBs2bLine->bs2b = bs2b_open();
	if(psBs2bLine->bs2b == NULL) {
		free(psBs2bLine);
		return NULL;
	}
	bs2b_set_srate(psBs2bLine->bs2b, rate);
	psBs2bLine->levelBackup = BS2B_DEFAULT_CLEVEL;

	psBs2bLine->alternatingBuffer = NULL;
	psBs2bLine->bufferSampleCount = 0;

	return (LV2_Handle)psBs2bLine;
}

/*****************************************************************************/

/* Initialise and activate a plugin instance. */
static void
activate(LV2_Handle instance) {
	/*
	Bs2bLine * psBs2bLine = (Bs2bLine *)Instance;
	NOOP
	*/
}

/*****************************************************************************/

/* Connect a port to a data location. */
static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data) {
	Bs2bLine * psBs2bLine = (Bs2bLine *)instance;
	switch ((PortIndex)port) {
	case LB_PORT_LOWPASS:
		psBs2bLine->m_pfLowpass = (float *) data;
		break;
	case LB_PORT_FEEDING:
		psBs2bLine->m_pfFeeding = (float *) data;
		break;
	case LB_PORT_INPUT_LEFT:
		psBs2bLine->m_pfInputLeft = (float *) data;
		break;
	case LB_PORT_INPUT_RIGHT:
		psBs2bLine->m_pfInputRight = (float *) data;
		break;
	case LB_PORT_OUTPUT_LEFT:
		psBs2bLine->m_pfOutputLeft = (float *) data;
		break;
	case LB_PORT_OUTPUT_RIGHT:
		psBs2bLine->m_pfOutputRight = (float *) data;
		break;
	}
}

/*****************************************************************************/

/* Run a bs2b instance for a block of SampleCount samples. */
static void
run(LV2_Handle instance, uint32_t n_samples) {
	float * pfInputLeft;
	float * pfInputRight;
	float * pfOutputLeft;
	float * pfOutputRight;
	Bs2bLine * psBs2bLine = (Bs2bLine *)instance;
	unsigned long lSampleIndex;

	uint16_t const lowpass = (uint16_t)psBs2bLine->m_pfLowpass[0];
	uint16_t const feeding = (uint16_t)(psBs2bLine->m_pfFeeding[0] * 10);
	uint32_t const currentLevel
		= LB_BETWEEN(BS2B_MINFCUT, lowpass, BS2B_MAXFCUT)
		| LB_BETWEEN(BS2B_MINFEED, feeding, BS2B_MAXFEED) << 16;

	pfInputLeft = psBs2bLine->m_pfInputLeft;
	pfInputRight = psBs2bLine->m_pfInputRight;
	pfOutputLeft = psBs2bLine->m_pfOutputLeft;
	pfOutputRight = psBs2bLine->m_pfOutputRight;

	/* bs2b_cross_feed_f increase cpu usage while all 0 inputs. 
	   First time (before using the plugin) does not increase cpus.
	   But once used, the cpu usage was up.
	   Below codes simply skip for avoiding this situations. 
	   I couldn't find other way for fix this. */
	
	for (lSampleIndex = 0; lSampleIndex < n_samples; lSampleIndex++) {
		if (pfInputLeft[lSampleIndex] != 0 || pfInputRight[lSampleIndex] != 0)
			break;
	}
	
	if (lSampleIndex == n_samples) {
		for (lSampleIndex = 0; lSampleIndex < n_samples; lSampleIndex++) {
			pfOutputLeft[lSampleIndex] = 0;
			pfOutputRight[lSampleIndex] = 0;
		}
		return;
	}

	/* Re-allocate when needed */
	if (n_samples > psBs2bLine->bufferSampleCount) {
		float * const reallocated
				= (float *) realloc(psBs2bLine->alternatingBuffer,
				sizeof(float) * n_samples * 2);
		if (reallocated) {
			psBs2bLine->alternatingBuffer = reallocated;
			psBs2bLine->bufferSampleCount = n_samples;
		} else {
			/* No suitable buffer to do processing with */
			free(psBs2bLine->alternatingBuffer);
			psBs2bLine->alternatingBuffer = NULL;
			psBs2bLine->bufferSampleCount = 0;
			return;
		}
	}

	/* Write to channel-alternating buffer */
	for (lSampleIndex = 0; lSampleIndex < n_samples; lSampleIndex++) {
		psBs2bLine->alternatingBuffer[2 * lSampleIndex + 0]
				= pfInputLeft[lSampleIndex];
		psBs2bLine->alternatingBuffer[2 * lSampleIndex + 1]
				= pfInputRight[lSampleIndex];
	}

	/* Sync settings */
	if (currentLevel != psBs2bLine->levelBackup) {
		bs2b_set_level(psBs2bLine->bs2b, currentLevel);
		psBs2bLine->levelBackup = currentLevel;
	}

	/* Apply effect */
	bs2b_cross_feed_f(psBs2bLine->bs2b, psBs2bLine->alternatingBuffer,
			n_samples);

	/* Read back from channel-alternating buffer */
	for (lSampleIndex = 0; lSampleIndex < n_samples; lSampleIndex++) {
		pfOutputLeft[lSampleIndex]
				= psBs2bLine->alternatingBuffer[2 * lSampleIndex + 0];
		pfOutputRight[lSampleIndex]
				= psBs2bLine->alternatingBuffer[2 * lSampleIndex + 1];
	}
}

/*****************************************************************************/

static void
deactivate(LV2_Handle instance)
{
}

/*****************************************************************************/

/* Throw away a bs2b line. */
static void
cleanup(LV2_Handle instance) {
	Bs2bLine * psBs2bLine = (Bs2bLine *)instance;
	if (psBs2bLine->bs2b != NULL) {
		bs2b_close(psBs2bLine->bs2b);
		psBs2bLine->bs2b = NULL;
	}

	free(psBs2bLine->alternatingBuffer);
	psBs2bLine->alternatingBuffer = NULL;

	free(psBs2bLine);
}


/*****************************************************************************/

static const void*
extension_data(const char* uri)
{
	return NULL;
}

/*****************************************************************************/

static const LV2_Descriptor descriptor = {
	BS2B_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

/*****************************************************************************/

/* Return a descriptor of the requested plugin type. Only one plugin
   type is available in this library. */

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index) {
	if (index == 0)
		return &descriptor;
	else
		return NULL;
}

/*****************************************************************************/

/* EOF */
