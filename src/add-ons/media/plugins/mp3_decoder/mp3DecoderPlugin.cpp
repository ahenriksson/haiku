#include <stdio.h>
#include <DataIO.h>
#include <Locker.h>
#include <MediaFormats.h>
#include "mp3DecoderPlugin.h"

#define TRACE_THIS 1
#if TRACE_THIS
  #define TRACE printf
#else
  #define TRACE ((void)0)
#endif

#define OUTPUT_BUFFER_SIZE	32768

mp3Decoder::mp3Decoder()
{
	InitMP3(&fMpgLibPrivate);
}

mp3Decoder::~mp3Decoder()
{
	ExitMP3(&fMpgLibPrivate);
}


status_t
mp3Decoder::Setup(media_format *ioEncodedFormat, media_format *ioDecodedFormat,
				  const void *infoBuffer, int32 infoSize)
{
	memset(ioDecodedFormat, 0, sizeof(*ioDecodedFormat));
	ioDecodedFormat->type = B_MEDIA_RAW_AUDIO;
	ioDecodedFormat->u.raw_audio.frame_rate = 44100;
	ioDecodedFormat->u.raw_audio.channel_count = 2;
	ioDecodedFormat->u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
	ioDecodedFormat->u.raw_audio.byte_order = B_MEDIA_LITTLE_ENDIAN;
	ioDecodedFormat->u.raw_audio.buffer_size = OUTPUT_BUFFER_SIZE;
	ioDecodedFormat->u.raw_audio.channel_mask = B_CHANNEL_LEFT | B_CHANNEL_RIGHT;
	return B_OK;
}

status_t
mp3Decoder::Seek(uint32 seekTo,
				 int64 seekFrame, int64 *frame,
				 bigtime_t seekTime, bigtime_t *time)
{
	ExitMP3(&fMpgLibPrivate);
	InitMP3(&fMpgLibPrivate);
	return B_OK;
}


status_t
mp3Decoder::Decode(void *buffer, int64 *frameCount,
				   media_header *mediaHeader, media_decode_info *info /* = 0 */)
{
	void *chunkBuffer;
	int32 chunkSize;
	if (B_OK != GetNextChunk(&chunkBuffer, &chunkSize, mediaHeader)) {
		TRACE("mp3Decoder::Decode: GetNextChunk failed\n");
			return B_ERROR;
	}

	int availsize;
	int outsize;
	int result;
	
	availsize = OUTPUT_BUFFER_SIZE;
	result = decodeMP3(&fMpgLibPrivate, (char *)chunkBuffer, chunkSize, (char *)buffer, availsize, &outsize);
	if (result == MP3_ERR) {
		TRACE("mp3Decoder::Decode: decodeMP3 returned MP3_ERR\n");
		return B_ERROR;
	}
	buffer = (char *)buffer + outsize;
	availsize -= outsize;

	do {
		result = decodeMP3(&fMpgLibPrivate, 0, 0, (char *)buffer, availsize, &outsize);
		buffer = (char *)buffer + outsize;
		availsize -= outsize;
		if (availsize < 0) {
			TRACE("mp3Decoder::Decode: decoded to much\n");
			exit(1);
		}
	} while (result == MP3_OK);
	
	*frameCount = (OUTPUT_BUFFER_SIZE - availsize) / 4;
	return B_OK;
}

Decoder *
mp3DecoderPlugin::NewDecoder()
{
	static BLocker locker;
	static bool initdone = false;
	locker.Lock();
	if (!initdone) {
		InitMpgLib();
		initdone = true;
	}
	locker.Unlock();
	return new mp3Decoder;
}

status_t
mp3DecoderPlugin::RegisterPlugin()
{
	PublishDecoder("audiocodec/mpeg1layer1", "mp3", "MPEG 1 audio layer 1 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg1layer2", "mp3", "MPEG 1 audio layer 2 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg1layer3", "mp3", "MPEG 1 audio layer 3 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2layer1", "mp3", "MPEG 2 audio layer 1 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2layer2", "mp3", "MPEG 2 audio layer 2 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2layer3", "mp3", "MPEG 2 audio layer 3 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2.5layer1", "mp3", "MPEG 2.5 audio layer 1 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2.5layer2", "mp3", "MPEG 2.5 audio layer 2 decoder, based on mpeg123 mpglib");
	PublishDecoder("audiocodec/mpeg2.5layer3", "mp3", "MPEG 2.5 audio layer 3 decoder, based on mpeg123 mpglib");
	return B_OK;
}


MediaPlugin *instantiate_plugin()
{
	return new mp3DecoderPlugin;
}
