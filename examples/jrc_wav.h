#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// various boilerplate taken from stb_*.h

#ifdef JRC_WAV_STATIC
#define JRCWDEF static
#else
#define JRCWDEF extern
#endif

#ifdef _MSC_VER
typedef unsigned short jrcw__uint16;
typedef   signed short jrcw__int16;
typedef unsigned int   jrcw__uint32;
typedef   signed int   jrcw__int32;
#else
#include <stdint.h>
typedef uint16_t jrcw__uint16;
typedef int16_t  jrcw__int16;
typedef uint32_t jrcw__uint32;
typedef int32_t  jrcw__int32;
#endif

// this is not threadsafe
static const char *jrcw__g_failure_reason;

JRCWDEF const char *jrcw_failure_reason(void)
{
   return jrcw__g_failure_reason;
}

static int jrcw__err(const char *str)
{
   jrcw__g_failure_reason = str;
   return 0;
}

#ifdef JRCW_NO_FAILURE_STRINGS
#define jrcw__err(x,y)  0
#elif defined(JRCW_FAILURE_USERMSG)
#define jrcw__err(x,y)  jrcw__err(y)
#else
#define jrcw__err(x,y)  jrcw__err(x)
#endif

#define jrcw__errpf(x,y)   ((float *)(size_t) (jrcw__err(x,y)?NULL:NULL))
#define jrcw__errpuc(x,y)  ((unsigned char *)(size_t) (jrcw__err(x,y)?NULL:NULL))

/******************************************************************************/

typedef struct wavChunkHeader_s
{
	char id[4];
	jrcw__uint32 size;
} wavChunkHeader_t;

typedef struct wavFmt_Data_s
{
	jrcw__uint16 formatTag;
	jrcw__uint16 channels;
	jrcw__uint32 sampleRate;
	jrcw__uint32 avgBytesPerSec;
	jrcw__uint16 blockAlign;
	jrcw__uint16 bitsPerSample;
} wavFmtData_t;

typedef struct wavSaveHeader_s
{
	wavChunkHeader_t riffHeader;
	char id_WAVE[4];
	wavChunkHeader_t fmt_Header;
	wavFmtData_t     fmt_Data;
	wavChunkHeader_t dataHeader;
} wavSaveHeader_t;

#define WRITE_FOURCC(dst, src) ((dst)[0] = (src)[0], (dst)[1] = (src)[1], (dst)[2] = (src)[2], (dst)[3] = (src)[3])
#define MATCH_FOURCC(dst, src) ((dst)[0] == (src)[0] && (dst)[1] == (src)[1] && (dst)[2] == (src)[2] && (dst)[3] == (src)[3])

JRCWDEF int JrcWav_Save(const char *filename, const unsigned char *data, size_t sizeInBytes, int channels, int sampleRate, int bitsPerSample)
{
	FILE *fp;
	wavSaveHeader_t head;

	WRITE_FOURCC(head.riffHeader.id, "RIFF");
	head.riffHeader.size = sizeInBytes + sizeof(head) - sizeof(head.riffHeader);
	WRITE_FOURCC(head.id_WAVE, "WAVE");

	WRITE_FOURCC(head.fmt_Header.id, "fmt ");
	head.fmt_Header.size = 16;
	head.fmt_Data.formatTag = 1; // Microsoft PCM format
	head.fmt_Data.channels = channels;
	head.fmt_Data.sampleRate = sampleRate;
	head.fmt_Data.avgBytesPerSec = sampleRate * channels * bitsPerSample / 8;
	head.fmt_Data.blockAlign = bitsPerSample / 2;
	head.fmt_Data.bitsPerSample = bitsPerSample;

	WRITE_FOURCC(head.dataHeader.id, "data");
	head.dataHeader.size = sizeInBytes;

	if (!(fp = fopen(filename, "wb")))
	{
		return jrcw__err("Can't fopen", "Unable to open file");
	}

	if (fwrite((unsigned char *)&head, 1, sizeof(head), fp) != sizeof(head))
	{
		fclose(fp);
		return jrcw__err("Can't fwrite header", "Can't fwrite header");
	}

	if (fwrite(data, 1, sizeInBytes, fp) != sizeInBytes)
	{
		fclose(fp);
		return jrcw__err("Can't fwrite data", "Can't fwrite data");
	}

	fclose(fp);

	return 1;
}

JRCWDEF int JrcWav_Load(const char *filename, unsigned char **data, int *sizeInBytes, int *channels, int *sampleRate, int *bitsPerSample)
{
	wavChunkHeader_t chunkHeader;
	char waveId[4];
	FILE *fp;

	if (!(fp = fopen(filename, "rb")))
	{
		return jrcw__err("Can't fopen", "Unable to open file");
	}

	if (fread(&chunkHeader, 1, sizeof(chunkHeader), fp) != sizeof(chunkHeader))
	{
		fclose(fp);
		return jrcw__err("Can't fread RIFF header", "Can't fread RIFF header");
	}

	if (!MATCH_FOURCC(chunkHeader.id, "RIFF"))
	{
		fclose(fp);
		return jrcw__err("File not RIFF", "File is not RIFF");
	}

	fread(waveId, 1, 4, fp);
	if (!MATCH_FOURCC(waveId, "WAVE"))
	{
		fclose(fp);
		return jrcw__err("File not WAVE", "File is not RIFF WAVE");
	}

	while (1)
	{
		size_t endPos;
		if (fread(&chunkHeader, 1, sizeof(chunkHeader), fp) != sizeof(chunkHeader))
			break;

		endPos = ftell(fp) + chunkHeader.size;

		if (MATCH_FOURCC(chunkHeader.id, "fmt "))
		{
			wavFmtData_t fmtData;

			if (chunkHeader.size < sizeof(fmtData))
			{
				//printf("fmt chunk too small, skipping\n");
			}
			else
			{
				fread(&fmtData, 1, sizeof(fmtData), fp);

				if (fmtData.formatTag != 1)
				{
					fclose(fp);
					return jrcw__err("File not MSPCM", "File is not Microsoft PCM");
				}

				if (channels)
					*channels = fmtData.channels;

				if (sampleRate)
					*sampleRate = fmtData.sampleRate;

				if (bitsPerSample)
					*bitsPerSample = fmtData.bitsPerSample;
			}
		}
		else if (MATCH_FOURCC(chunkHeader.id, "data"))
		{
			if (data)
			{
				*data = (unsigned char *)malloc(chunkHeader.size);
				chunkHeader.size = fread(*data, 1, chunkHeader.size, fp);
			}

			if (sizeInBytes)
				*sizeInBytes = chunkHeader.size;
		}

		fseek(fp, endPos, SEEK_SET);
	}

	fclose(fp);

	return 1;
}

#ifdef __cplusplus
}
#endif
