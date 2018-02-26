#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jrc_wav.h"

#define RESWEEP_IMPLEMENTATION
#include "resweep.h"

int64_t getTimerTicksMs()
{
#ifdef WIN32
	static LONGLONG ticksPerSec = 0;
	LARGE_INTEGER pc;

	if (!ticksPerSec)
	{
		QueryPerformanceFrequency(&pc);
		ticksPerSec = pc.QuadPart;
	}

	QueryPerformanceCounter(&pc);

	return pc.QuadPart * 1000ll / ticksPerSec;
#else
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000ll + ts.tv_nsec / 1000000ll;
#endif
}

int main(int argc, char *argv[])
{
	unsigned char *wavIn, *wavOut;
	char *filenameIn = NULL, *filenameOut = NULL;
	int sizeIn, sizeOut;
	int channels, freqIn, freqOut, bits;
	unsigned int gcd;
	int64_t startTime, endTime;
	double elapsedSeconds;
	int i;

	freqOut = 0;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			{
				if (filenameOut)
				{
					printf("Error: Multiple output files!\n");
					return 0;
				}
				i++;
				filenameOut = argv[i];
			}

			if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
			{
				if (freqOut)
				{
					printf("Error: Multiple output frequencies!\n");
					return 0;
				}
				i++;
				freqOut = atof(argv[i]);
			}
		}
		else
		{
			if (filenameIn)
			{
				printf("Error: Multiple input files!\n");
				return 0;
			}
			filenameIn = argv[i];
		}
	}

	if (!filenameIn)
	{
		printf("Error: No input file!\n");
		return 0;
	}

	if (!filenameOut)
	{
		printf("Defaulting output file to output.wav\n");
		filenameOut = "output.wav";
	}

	if (!freqOut)
	{
		printf("Defaulting output frequency to 44100\n");
		freqOut = 44100;
	}

	printf("input \"%s\" output \"%s\"\n", filenameIn, filenameOut);

	if (!JrcWav_Load(filenameIn, &wavIn, &sizeIn, &channels, &freqIn, &bits))
	{
		printf("Error opening %s: %s\n", filenameIn, jrcw_failure_reason());
		return 0;
	}

	if (channels > 2)
	{
		printf("Channels = %d, only 1 or 2 supported (for now).\n", channels);
		return 0;
	}

	if (bits != 16)
	{
		printf("Bits = %d, only 16 supported (for now).\n", bits);
		return 0;
	}

	printf("converting %dhz to %dhz\n", freqIn, freqOut);

	gcd = calc_gcd(freqOut, freqIn);

	sizeOut = sizeIn * (long long)(freqOut / gcd) / (long long)(freqIn / gcd);
	wavOut = (unsigned char *)malloc(sizeOut);

	fflush(stdout);

	startTime = getTimerTicksMs();

	sinc_resample((short *)wavOut, sizeOut, freqOut, (short *)wavIn, sizeIn, freqIn, channels);

	endTime = getTimerTicksMs();

	JrcWav_Save(filenameOut, wavOut, sizeOut, channels, freqOut, 16);

	elapsedSeconds = (endTime - startTime) / 1000.0;

	printf("Done in %g sec, x%g realtime!\n", elapsedSeconds, sizeIn / 2 / channels / (double)freqIn / elapsedSeconds);

	free(wavIn);
	free(wavOut);
}
