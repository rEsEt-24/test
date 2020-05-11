#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "timing.h"
#include "shine_mp3.h"

#define DR_WAV_IMPLEMENTATION

#include "dr_wav.h"

#define DR_MP3_IMPLEMENTATION

#include "dr_mp3.h"

#define NK_Int int

typedef enum
{
    FORMAT_S16,
    FORMAT_S32,
	FORMAT_FLOAT
}WAV_FORMAT_E;


uint64_t mp3TotalSampleCount,wavTotalSampleCount,test_count;
uint32_t testChannels,testSampleRate; 

void error(char *s);

/* Output error message and exit */
void error(char *s) {
    fprintf(stderr, "Error: %s\n", s);
    exit(1);
}

//获取文件的大小 
int get_file_size(int f)
{
    struct stat st;
    fstat(f, &st);
    return st.st_size;
}

// Write out the MP3 file
int write_mp3_to_file(FILE *outfile,long bytes, void *buffer, void *config) 
{
    return fwrite(buffer, sizeof(unsigned char), bytes, outfile) / sizeof(unsigned char);
}

// Write out the MP3 file
int write_mp3_to_memory(void *outBuffer,long bytes, uint64_t *totalWriteCount ,void *intBuffer, void *config) 
{
	uint64_t *iTotalWriteCount = totalWriteCount;
		
	*iTotalWriteCount += bytes;
	outBuffer = realloc(outBuffer,*iTotalWriteCount);
	if(outBuffer == NULL)
	{
		printf("outBuffer recolloc error!\n");
		return -1;
	}
	else
	{
		memcpy(outBuffer+*iTotalWriteCount-bytes,intBuffer,bytes);
	}
	return 0;
}


// Write out the MP3 file  
int wavWrite_int16(FILE *outfile, 
						void *buffer,
						uint32_t sampleRate, 
						uint64_t totalSampleCount, 
						int channels, 
						void *config)
{
	FILE *fp = outfile;
	unsigned char *pBuffer = buffer;
	uint64_t iTotalSampleCount = totalSampleCount;
	uint32_t iChannels = channels;
	uint32_t iSampleRate = sampleRate;
	
	//修正写入的buffer长度
    iTotalSampleCount *= sizeof(int16_t)*iChannels;
	printf("the wavWrite_int16 iTotalSampleCount is %d\n",iTotalSampleCount);
    int nbit = 16;
    int FORMAT_PCM = 1;
    int nbyte = nbit / 8;
    char text[4] = { 'R', 'I', 'F', 'F' };
    uint32_t long_number = 36 + iTotalSampleCount;
    fwrite(text, 1, 4, fp);
    fwrite(&long_number, 4, 1, fp);
    text[0] = 'W';
    text[1] = 'A';
    text[2] = 'V';
    text[3] = 'E';
    fwrite(text, 1, 4, fp);
    text[0] = 'f';
    text[1] = 'm';
    text[2] = 't';
    text[3] = ' ';
    fwrite(text, 1, 4, fp);

    long_number = 16;
    fwrite(&long_number, 4, 1, fp);
    int16_t short_number = FORMAT_PCM;//默认音频格式
    fwrite(&short_number, 2, 1, fp);
    short_number = iChannels; // 音频通道数
    fwrite(&short_number, 2, 1, fp);
    long_number = iSampleRate; // 采样率
    fwrite(&long_number, 4, 1, fp);
    long_number = iSampleRate * nbyte; // 比特率
    fwrite(&long_number, 4, 1, fp);
    short_number = nbyte; // 块对齐
    fwrite(&short_number, 2, 1, fp);
    short_number = nbit; // 采样精度
    fwrite(&short_number, 2, 1, fp);
    char data[4] = { 'd', 'a', 't', 'a' };
    fwrite(data, 1, 4, fp);
    long_number = iTotalSampleCount;
    fwrite(&long_number, 4, 1, fp);
	fwrite(pBuffer, iTotalSampleCount, 2, fp);	
	return 0;
}


/* Use these default settings, can be overridden */
static void set_defaults(shine_config_t *config) {
    shine_set_config_mpeg_defaults(&config->mpeg);
}

static void print_usage(char *infname,char *outfname) 
{
    printf("Audio Processing\n");
    printf("mp3 encoder && decoder\n");
    printf("blog: http://cpuimage.cnblogs.com/\n");
    printf("Usage: tinymp3 <%s> <%s>\n\n",infname,outfname);
#if 0 
    printf("Use \"-\" for standard input or output.\n\n");
    printf("Options:\n");
    printf(" -h            this help message\n");
    printf(" -b <bitrate>  set the bitrate [8-320], default 64 kbit\n");
    printf(" -m            force encoder to operate in mono\n");
    printf(" -c            set copyright flag, default off\n");
    printf(" -j            encode in joint stereo (stereo pData only)\n");
    printf(" -d            encode in dual-channel (stereo pData only)\n");
    printf(" -q            quiet mode\n");
#endif
}

/* Print some info about what we're going to encode */
static void check_config(shine_config_t *config) {
    static char *version_names[4] = {"2.5", "reserved", "II", "I"};
    static char *mode_names[4] = {"stereo", "joint-stereo", "dual-channel", "mono"};
    static char *demp_names[4] = {"none", "50/15us", "", "CITT"};

    printf("MPEG-%s layer III, %s  Psychoacoustic Model: Shine\n",
           version_names[shine_check_config(config->wave.samplerate, config->mpeg.bitr)],
           mode_names[config->mpeg.mode]);
    printf("Bitrate: %d kbps  ", config->mpeg.bitr);
    printf("De-emphasis: %s   %s %s\n",
           demp_names[config->mpeg.emph],
           ((config->mpeg.original) ? "Original" : ""),
           ((config->mpeg.copyright) ? "(C)" : ""));
}


/**
 * 把输入的pInfname wav格式音频文件转换成 pOutfnameMP3格式的音频文件
 * @brief 音频转换（WAV格式文件转MP3格式文件）
 * @param infname，输入的pInfname wav格式音频文件
 * @param outfname，输出pOutfnameMP3格式的音频文件
 * @param wavFormat 输入的wav格式音频文件的bit位   默认为定点16位：FORMAT_S16
 * @return NK_Int, 成功返回0，失败返回-1
 * @note 
 * @see NK_AUDIO_WavToMp3_FileConvert
 **/
NK_Int NK_AUDIO_WavToMp3_FileConvert(	char *infname,char *outfname,char wavFormat)
{
	FILE *outfile;
	char *pIntfilename = infname;
	char *pOutfilename = outfname;
	char iWavFormat = wavFormat;
	shine_config_t config;
	shine_t s;
    int iWritten,iSamples_per_pass;
    unsigned char *pData;
	int16_t *pBuffer,*pCache,*pData_in;
	uint32_t iSampleRate = 0;
    uint64_t iTotalSampleCount = 0;
    uint32_t iChannels = 0;
	size_t iCount,iLast;
	double time_interval;
	double startTime;
	
	set_defaults(&config);		//bitr = 64,emph = NONE,copyright = 0,original = 1.

	startTime = now();
	print_usage(pIntfilename,pOutfilename);
    printf("Encoding \"%s\" to \"%s\"\n", pIntfilename, pOutfilename);

	switch (iWavFormat)
	{
		case FORMAT_S16:
			pData_in = drwav_open_and_read_file_s16(pIntfilename, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
#if 0		
		case FORMAT_S32:
			//pData_in = (int32_t *)drwav_open_and_read_file_s32(pIntfilename, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
		
		case FORMAT_FLOAT:
			//pData_in = drwav_open_and_read_file_f32(pIntfilename, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
#endif		
		default:
			printf("format is wrong!!!");
			return -1;
		break;
	}
	
	if (pData_in == NULL)
	{
		printf("read file [%s] error.\n", pIntfilename);
		return -1;
	}
	
	config.wave.samplerate = iSampleRate;
	config.wave.channels = iChannels;
	
	/* See if samplerate and bitrate are valid */
	if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0)
	{
		error("Unsupported samplerate/bitrate configuration.");
	}
	
	/* open the output file */
	outfile = fopen(pOutfilename, "wb");
    if (outfile == NULL) 
	{
        fprintf(stderr, "Could not create \"%s\".\n", pOutfilename);
		free(pData_in);
        return -1;
    }	
		
	/* Set to stereo mode if wave pData is stereo, mono otherwise. */
	//config.mpeg.mode = (config.wave.channels > 1)?  stereo:MONO;
	if (config.wave.channels > 1)
	{
        config.mpeg.mode = STEREO;
	}
    else
	{
        config.mpeg.mode = MONO;
	}
	
	/* Initiate encoder */
	s = shine_initialise(&config);
	check_config(&config);

	iSamples_per_pass = shine_samples_per_pass(s) * iChannels;

	/* All the magic happens here */
	iCount = iTotalSampleCount / iSamples_per_pass;
	pBuffer = (int16_t *)pData_in;	
	for (int i = 0; i < iCount; i++) 
	{
		pData = shine_encode_buffer_interleaved(s, pBuffer, &iWritten);
		if (write_mp3_to_file(outfile, iWritten, pData, &config) != iWritten) 
		{
			fprintf(stderr, "mp3 encoder && decoder: write error\n");
			return -1;
		}
		pBuffer += iSamples_per_pass;
	}
	iLast = iTotalSampleCount % iSamples_per_pass;
	if (iLast != 0) 
	{
		pCache = (int16_t *) calloc(iSamples_per_pass, sizeof(int16_t));
		if (pCache != NULL) 
		{
			memcpy(pCache, pBuffer, iLast * sizeof(int16_t));
			pData = shine_encode_buffer_interleaved(s, pCache, &iWritten);
			free(pCache);
			if (write_mp3_to_file(outfile, iWritten, pData, &config) != iWritten) 
			{
				fprintf(stderr, "mp3 encoder && decoder: write error\n");
				return -1;
			}
		}
		else
		{
			printf("calloc for iSamples_per_pass is error!!!\n");
			free(pData_in);
			return -1;
		}
	}

	/* Flush and write remaining pData. */
	pData = shine_flush(s, &iWritten);
	write_mp3_to_file(outfile, iWritten, pData, &config);

	/* Close encoder. */
	shine_close(s);
	/* Close the MP3 file */
	fclose(outfile);
	free(pData_in);

	time_interval = calcElapsed(startTime, now());
    printf("time interval: %d ms\n ", (int) (time_interval * 1000));
	wavTotalSampleCount = iTotalSampleCount;
	return 0;
}


/**
 * 把输入的pInfname MP3格式音频文件转换成 pOutfname wav格式的音频文件
 * @brief 音频转换（MP3格式文件转WAV格式文件）
 * @param infname，输入的pInfname MP3格式音频文件
 * @param infname，输出pOutfname wav格式的音频文件
 * @return NK_Int, 成功返回0，失败返回-1
 * @note 
 * @see NK_AUDIO_Mp3ToWav_FileConvert
 **/
NK_Int NK_AUDIO_Mp3ToWav_FileConvert(	char *infname,char *outfname)
{
	FILE *outfile;
	char *pIntfilename = infname;
	char *pOutfilename = outfname;
	shine_config_t config;
	drmp3_config mp3Config;	
	float *pMp3_buffer;
	int16_t *pBuffer;
	uint32_t iSampleRate = 0;
    uint32_t iChannels = 0;
    uint64_t iTotalSampleCount = 0;
	double time_interval;
	double startTime;

	set_defaults(&config);		//bitr = 64,emph = NONE,copyright = 0,original = 1.
	drmp3_zero_object(&mp3Config);

	startTime = now();
	print_usage(pIntfilename,pOutfilename);
    printf("Encoding \"%s\" to \"%s\"\n", pIntfilename, pOutfilename);

	pMp3_buffer = drmp3_open_and_decode_file_f32(pIntfilename, &mp3Config, &iTotalSampleCount);
	if (pMp3_buffer != NULL) 
	{
		iChannels = mp3Config.outputChannels;
		iSampleRate = mp3Config.outputSampleRate;
		pBuffer = (int16_t *) calloc(iTotalSampleCount, sizeof(int16_t));
		if (pBuffer != NULL)
		{
			drwav_f32_to_s16(pBuffer, pMp3_buffer, iTotalSampleCount);	//把32浮点格式转换成16位定点格式
		}
		else
		{
			printf("calloc for iTotalSampleCount is error\n!!!");
			free(pMp3_buffer);
			return -1;
		}
		free(pMp3_buffer);
	} 
	else 
	{
		printf("read file [%s] error.\n", pIntfilename);
		return -1;
	}
	printf("iTotalSampleCount = %d\n",iTotalSampleCount);
	/* open the output file */
	outfile = fopen(pOutfilename, "wb");
    if (outfile == NULL) 
	{
        fprintf(stderr, "Could not create \"%s\".\n", pOutfilename);
        return -1;
    }	
	if(wavWrite_int16(outfile,pBuffer,iSampleRate,iTotalSampleCount,iChannels,&config)<0)
	{
		fprintf(stderr, "write_wav : write error\n");
		return -1;
	}
	
	/* Close the WAV file */
	fclose(outfile);
	free(pBuffer);

	time_interval = calcElapsed(startTime, now());
    printf("time interval: %d ms\n ", (int) (time_interval * 1000));
	mp3TotalSampleCount = iTotalSampleCount;
	return 0;	
}


/**
 * 把输入的inputbuffer中 wav格式音频文件转换成 outputbuffer中MP3格式的音频文件
 * @brief 音频转换（WAV格式文件转MP3格式文件）
 * @param pInfname，输入的pInfname wav格式音频文件
 * @param pOutfname，输出pOutfnameMP3格式的音频文件
 * @param wavFormat_e 输入的wav格式音频文件的bit位   默认为定点16位：FORMAT_S16
 * @return NK_Int, 成功返回0，失败返回-1
 * @note 
 * @see NK_AUDIO_WavToMp3_BufferConvert
 **/
NK_Int NK_AUDIO_WavToMp3_BufferConvert(	void *intputBuffer,
													size_t intDataSize,
													void *outputBuffer,
//													size_t outDataSize,
													char wavFormat)
{
	unsigned char *pIntputBuffer = intputBuffer;
	size_t iIntDataSize = intDataSize;
	unsigned char *pOutputBuffer = outputBuffer;
	char iWavFormat = wavFormat;
	shine_config_t config;
	shine_t s;
    int iWritten,iSamples_per_pass;
    unsigned char *pData;
	int16_t *pBuffer,*pCache,*pData_in;
	uint32_t iSampleRate = 0;
    uint64_t iTotalSampleCount = 0;
    uint32_t iChannels = 0;
	size_t iCount,iLast;
	double time_interval;
	double startTime;
	uint64_t iTotalWriteCount = 0;
	uint64_t iHasWriteCount = 0;
	
	set_defaults(&config);		//bitr = 64,emph = NONE,copyright = 0,original = 1.

	startTime = now();

	switch (iWavFormat)
	{
		case FORMAT_S16:
			pData_in = drwav_open_and_read_memory_s16(pIntputBuffer,iIntDataSize, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
#if 0		
		case FORMAT_S32:
			//pData_in = (int32_t *)drwav_open_and_read_memory_s32(pIntputBuffer,iIntDataSize, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
		
		case FORMAT_FLOAT:
			//pData_in = drwav_open_and_read_memory_f32(pIntputBuffer,iIntDataSize, &iChannels, &iSampleRate, &iTotalSampleCount);
		break;
#endif		
		default:
			printf("format is wrong!!!");
			return -1;
		break;
	}
	
	if (pData_in == NULL)
	{
		printf("read intputBuffer memory erro !\n");
		return -1;
	}
	
	config.wave.samplerate = iSampleRate;
	config.wave.channels = iChannels;
	
	/* See if samplerate and bitrate are valid */
	if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0)
	{
		error("Unsupported samplerate/bitrate configuration.");
	}

	/* Set to stereo mode if wave pData is stereo, mono otherwise. */
	//config.mpeg.mode = (config.wave.channels > 1)?  stereo:MONO;
	if (config.wave.channels > 1)
	{
        config.mpeg.mode = STEREO;
	}
    else
	{
        config.mpeg.mode = MONO;
	}
	
	/* Initiate encoder */
	s = shine_initialise(&config);
	check_config(&config);

	iSamples_per_pass = shine_samples_per_pass(s) * iChannels;

	/* All the magic happens here */
	iCount = iTotalSampleCount / iSamples_per_pass;
	pBuffer = (int16_t *)pData_in;	
	iTotalWriteCount = 0;
	
	for (int i = 0; i < iCount; i++) 
	{
		pData = shine_encode_buffer_interleaved(s, pBuffer, &iWritten);
		if(write_mp3_to_memory(pOutputBuffer,iWritten,&iTotalWriteCount,pData,&config)<0)
		{
			printf("write_mp3_to_memory error !!!\n");
			return -1;
		}
		pBuffer += iSamples_per_pass;
	}
	iLast = iTotalSampleCount % iSamples_per_pass;
	if (iLast != 0) 
	{
		pCache = (int16_t *) calloc(iSamples_per_pass, sizeof(int16_t));
		if (pCache != NULL) 
		{
			memcpy(pCache, pBuffer, iLast * sizeof(int16_t));
			pData = shine_encode_buffer_interleaved(s, pCache, &iWritten);
			free(pCache);
			if(write_mp3_to_memory(pOutputBuffer,iWritten,&iTotalWriteCount,pData,&config)<0)
			{
				printf("write_mp3_to_memory error !!!\n");
				return -1;
			}	
		}
		else
		{
			printf("calloc for iSamples_per_pass is error!!!\n");
			free(pData_in);
			return -1;
		}
	}

	/* Flush and write remaining pData. */
	pData = shine_flush(s, &iWritten);
	if(write_mp3_to_memory(pOutputBuffer,iWritten,&iTotalWriteCount,pData,&config)<0)
	{
		printf("write_mp3_to_memory error !!!\n");
		return -1;
	}

	/* Close encoder. */
	shine_close(s);
	free(pData_in);

	time_interval = calcElapsed(startTime, now());
    printf("time interval: %d ms\n ", (int) (time_interval * 1000));
	return 0;
}


/**
 * 把输入的pInfname MP3格式音频文件转换成 pOutfname wav格式的音频文件
 * @brief 音频转换（MP3格式文件转WAV格式文件）
 * @param pInfname，输入的pInfname MP3格式音频文件
 * @param pOutfname，输出pOutfname wav格式的音频文件
 * @return NK_Int, 成功返回0，失败返回-1
 * @note 
 * @see NK_AUDIO_Mp3ToWav_FileConvert
 **/
NK_Int NK_AUDIO_Mp3ToWav_BufferConvert(void *intputBuffer,size_t intDataSize,void *outputBuffer)
{
	unsigned char *pIntputBuffer = intputBuffer;
	size_t iIntDataSize = intDataSize;
	int16_t *pOutputBuffer = outputBuffer;
	shine_config_t config;
	drmp3_config mp3Config;	
	float *pMp3_buffer;
	int16_t *pBuffer;
	uint32_t iSampleRate = 0;
    uint32_t iChannels = 0;
    uint64_t iTotalSampleCount = 0;
	double time_interval;
	double startTime;

	set_defaults(&config);		//bitr = 128,emph = NONE,copyright = 0,original = 1.
	drmp3_zero_object(&mp3Config);

	startTime = now();
					
	pMp3_buffer = drmp3_open_and_decode_memory_f32(pIntputBuffer,iIntDataSize,&mp3Config, &iTotalSampleCount);
	if (pMp3_buffer != NULL) 
	{
		iChannels = mp3Config.outputChannels;
		iSampleRate = mp3Config.outputSampleRate;
		pBuffer = (int16_t *) calloc(iTotalSampleCount, sizeof(int16_t));
		if (pBuffer != NULL)
		{
			drwav_f32_to_s16(pBuffer, pMp3_buffer, iTotalSampleCount);	//把32浮点格式转换成16位定点格式
		}
		else
		{
			printf("calloc for iTotalSampleCount is error\n!!!");
			free(pMp3_buffer);
			return -1;
		}
		free(pMp3_buffer);
	} 
	else 
	{
		printf("drmp3_open_and_decode_memory_f32 error.\n");
		return -1;
	}

	// 存储到输出缓存区内存 
	pOutputBuffer = (int16_t *)calloc((iTotalSampleCount*sizeof(int16_t)*iChannels),sizeof(int16_t));
	if(pOutputBuffer == NULL)
	{
		printf("pOutputBuffer malloc error!!!\n");
		return -1;
	}
	else
	{
		memcpy(pOutputBuffer,pBuffer,(iTotalSampleCount*sizeof(int16_t)*iChannels));
		printf("memcpy to testWavBuffer ok\n");
	}

	//释放解码数据内存指针
	free(pBuffer);

	time_interval = calcElapsed(startTime, now());
    printf("time interval: %d ms\n ", (int) (time_interval * 1000));
	test_count = iTotalSampleCount;
	testChannels = iChannels;
	testSampleRate = iSampleRate;
	return 0;	
}


int main(int argc, char **argv) {
    char *infname;
    char *outfname;
	char wavFormat_e;
	int16_t *wavIntBuffer;
	unsigned char *mp3IntBuffer;
    int16_t *wavOutBuffer;
	unsigned char *mp3OutBuffer;

	shine_config_t config;
	FILE *outfile,*mp3file,*wavfile;
	
	if (argc < 3) 
	{
		printf("please intput intputfile outputfile wavformat!\n");
		return 0;
	}

    infname = argv[1];
    outfname = argv[2];
	wavFormat_e = atoi(argv[3]);
	
	if(NK_AUDIO_WavToMp3_FileConvert(infname,outfname,wavFormat_e)<0)
	{
		printf("wav file convert to mp3 file fualt!!!\n");
		printf("try to convert mp3 file to wav file!!!\n");
		if(NK_AUDIO_Mp3ToWav_FileConvert(infname,outfname)<0)
		{
			printf("mp3 file convert to wav file fualt!!!\n");
		}
		else
		{
			printf("mp3 file convert to wav file success!!!\n");
		}
	}
	else
	{
		printf("wav file convert to mp3 file success!!!\n");
	}

	/* open the output file */
	mp3file = fopen("./wolf.mp3", "r");
	if (mp3file == NULL) 
	{
		fprintf(stderr, "Could not create ./wolf.mp3\n");
		return -1;
	}	
	
	printf("mp3TotalSampleCount=%d\n",mp3TotalSampleCount);
	mp3TotalSampleCount = get_file_size(fileno(mp3file));
	printf("mp3TotalSampleCount=%d\n",mp3TotalSampleCount);

	mp3IntBuffer = malloc(mp3TotalSampleCount);			
	fread(mp3IntBuffer,mp3TotalSampleCount,1,mp3file);

	
//	printf("mp3IntBuffer=%s\n",mp3IntBuffer);
//	wavfile = fopen("./abcdefg.wav", "wb");
//	fwrite(mp3IntBuffer,1,mp3TotalSampleCount*2*sizeof(int16_t),wavfile);

	
//	fread(wavBuffer,2,wavTotalSampleCount,infname);
	
//	if()
//	NK_AUDIO_WavToMp3_BufferConvert();
	
	if(NK_AUDIO_Mp3ToWav_BufferConvert(mp3IntBuffer,mp3TotalSampleCount,wavOutBuffer)<0)
	{
		printf("NK_AUDIO_Mp3ToWav_BufferConvert error!!\n");
	}
	else
	{
		/* open the output file */
		outfile = fopen("./abcdefg.wav", "wb");
		if (outfile == NULL) 
		{
			fprintf(stderr, "Could not create ./abcdefg.wav\n");
			return -1;
		}	
		if(wavWrite_int16(outfile,wavOutBuffer,testSampleRate,test_count,testChannels,&config)<0)
		{
			fprintf(stderr, "write_wav : write error\n");
			return -1;
		}
		fclose(outfile);
		//free(wavOutBuffer);
	}

	
	/* Close the WAV file */
	
	fclose(mp3file);
//	free(testWavBuffer);
	free(mp3IntBuffer);
	
	return 0;
}


 




