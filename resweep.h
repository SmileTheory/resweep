#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#ifdef RESWEEP_IMPLEMENTATION

#include <math.h>

#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif

#ifndef M_1_PI
#define	M_1_PI 0.31830988618379067154
#endif

// 7 bits is good enough for 44.1khz -> 48khz upsampling
// 8 bits is good for 1:2 upsampling
// 10 bits is good for 2:1 downsampling
// 12 bits is good for 4:1 downsampling
// 14 bits is good for 8:1 downsampling
#define MAX_SINC_WINDOW_BITS 12
#define MAX_SINC_WINDOW_SIZE (1 << MAX_SINC_WINDOW_BITS)

#define RESAMPLE_LUT_STEP 96

typedef struct
{
	float value;
	float delta;
}
lutEntry_t;

lutEntry_t dynamicLut[RESAMPLE_LUT_STEP * MAX_SINC_WINDOW_SIZE];

static inline unsigned int calc_gcd(unsigned int a, unsigned int b)
{
	while (b)
	{
		unsigned int t = b;
		b = a % b;
		a = t;
	}

	return a;
}

static inline double exact_nsinc(double x)
{
	if (x == 0.0 || x == -0.0)
		return 1.0;

	return ((double)(M_1_PI) / x) * sin(M_PI * x);
}

static inline double exact_blackman_harris(double x)
{
	if (x < 0.0 || x > 1.0)
		return 0.0;

	return 0.35875 - 0.48829 * cos(2.0 * M_PI * x) + 0.14128 * cos(4.0 * M_PI * x) - 0.01168 * cos(6.0 * M_PI * x);
}

static inline void sinc_resample_createLut(double freqAdjust, int windowSize)
{
	lutEntry_t *out, *in;
	int i, j;

	if (freqAdjust > 1.0) freqAdjust = 1.0;

	out = dynamicLut;
	for (i = 0; i < RESAMPLE_LUT_STEP; i++)
	{
		for (j = 0; j < windowSize; j++)
		{
			double npos = j - windowSize / 2 + i / (double)(RESAMPLE_LUT_STEP - 1);
			double bpos = npos * (double)(1.0 / windowSize) + 0.5;
			double s = exact_nsinc(npos * freqAdjust) * freqAdjust;
			double w = exact_blackman_harris(bpos);
			out->value = s * w;
			out++;
		}
	}

	out = dynamicLut;
	in = out + windowSize;
	for (i = 0; i < RESAMPLE_LUT_STEP - 1; i++)
	{
		for (j = 0; j < windowSize; j++)
		{
			out->delta = in->value - out->value;
			out++;
			in++;
		}
	}

	for (j = 0; j < windowSize; j++)
	{
		out->delta = 0;
		out++;
	}
}

static inline void sinc_resample_internal(short *wavOut, int sizeOut, int outFreq, const short *wavIn, int sizeIn, int inFreq, int numChannels, int windowSize)
{
	float y[windowSize * numChannels];
	const short *sampleIn, *wavInEnd = wavIn + (sizeIn / 2);
	short *sampleOut, *wavOutEnd = wavOut + (sizeOut / 2);
	float outPeriod, freqAdjust;
	int subpos = 0;
	int gcd = calc_gcd(inFreq, outFreq);
	int i, c, next;
	float dither[numChannels];

	inFreq /= gcd;
	outFreq /= gcd;
	outPeriod = 1.0f / outFreq;

	freqAdjust = (inFreq > outFreq) ? ((float)outFreq / (float)inFreq) : 1.0f;

	sinc_resample_createLut(freqAdjust, windowSize);

	for (c = 0; c < numChannels; c++)
		dither[c] = 0.0f;

	for (i = 0; i < windowSize / 2 - 1; i++)
	{
		for (c = 0; c < numChannels; c++)
			y[i * numChannels + c] = 0;
	}

	sampleIn = wavIn;
	for (; i < windowSize; i++)
	{
		for (c = 0; c < numChannels; c++)
			y[i * numChannels + c] = (sampleIn < wavInEnd) ? *sampleIn++ : 0;
	}

	sampleOut = wavOut;
	next = 0;
	while (sampleOut < wavOutEnd)
	{
		float samples[numChannels];
		float offset = 1.0f - subpos * outPeriod;
		float interp;
		lutEntry_t *lutPart;
		int index;

		for (c = 0; c < numChannels; c++)
			samples[c] = 0.0f;

		interp = offset * (RESAMPLE_LUT_STEP - 1);
		index = interp;
		interp -= index;
		lutPart = dynamicLut + index * windowSize;

		for (i = next; i < windowSize; i++, lutPart++)
		{
			float scale = lutPart->value + lutPart->delta * interp;

			for (c = 0; c < numChannels; c++)
				samples[c] += y[i * numChannels + c] * scale;
		}

		for (i = 0; i < next; i++, lutPart++)
		{
			float scale = lutPart->value + lutPart->delta * interp;

			for (c = 0; c < numChannels; c++)
				samples[c] += y[i * numChannels + c] * scale;
		}

		for (c = 0; c < numChannels; c++)
		{
			float r = roundf(samples[c] + dither[c]);
			dither[c] += samples[c] - r;

			if (r > 32767)
				*sampleOut++ = 32767;
			else if (r < -32768)
				*sampleOut++ = -32768;
			else
				*sampleOut++ = r;
		}

		subpos += inFreq;
		while (subpos >= outFreq)
		{
			subpos -= outFreq;

			for (c = 0; c < numChannels; c++)
				y[next * numChannels + c] = (sampleIn < wavInEnd) ? *sampleIn++ : 0;

			next = (next + 1) % windowSize;
		}
	}
}

void sinc_resample(short *wavOut, int sizeOut, int outFreq, const short *wavIn, int sizeIn, int inFreq, int numChannels)
{
	float ratio;
	int windowSize;

	// Just copy if no resampling necessary
	if (outFreq == inFreq)
	{
		memcpy(wavOut, wavIn, (sizeOut < sizeIn) ? sizeOut : sizeIn);
		return;
	}

	ratio = (float)outFreq / (float)inFreq;

	// completely ad-hoc window size calculation
	if (ratio > 1.0f)
		ratio = 1.0f;

	windowSize = 1 << (int)(8 - log2f(ratio) * 2.0);
	if (windowSize > MAX_SINC_WINDOW_SIZE)
		windowSize = MAX_SINC_WINDOW_SIZE;

	// should compile as different paths
	// number of channels need to be compiled as separate paths to ensure good
	// vectorization by the compiler
	if (numChannels == 1)
		sinc_resample_internal(wavOut, sizeOut, outFreq, wavIn, sizeIn, inFreq, 1, windowSize);
	else if (numChannels == 2)
		sinc_resample_internal(wavOut, sizeOut, outFreq, wavIn, sizeIn, inFreq, 2, windowSize);
	else
		sinc_resample_internal(wavOut, sizeOut, outFreq, wavIn, sizeIn, inFreq, numChannels, windowSize);

}

#endif // RESWEEP_IMPLEMENTATION
