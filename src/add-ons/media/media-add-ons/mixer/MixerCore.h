#ifndef _MIXER_CORE_H
#define _MIXER_CORE_H

#include <Locker.h>

class AudioMixer;
class MixerInput;
class MixerOutput;
class Resampler;

class MixerCore
{
public:
	MixerCore(AudioMixer *node);
	~MixerCore();
	
	bool AddInput(const media_input &input);
	bool AddOutput(const media_output &output);

	bool RemoveInput(int32 inputID);
	bool RemoveOutput();
	
	int32 CreateInputID();
	
	MixerInput *Input(int index); // index = 0 to count-1, NOT inputID
	MixerOutput *Output();
	
	void Lock();
	bool LockWithTimeout(bigtime_t timeout);
	void Unlock();
	
	void BufferReceived(BBuffer *buffer, bigtime_t lateness);
	
	void InputFormatChanged(int32 inputID, const media_multi_audio_format &format);
	void OutputFormatChanged(const media_multi_audio_format &format);

	void SetOutputBufferGroup(BBufferGroup *group);
	void SetTimingInfo(BTimeSource *ts, bigtime_t downstream_latency);
	void EnableOutput(bool enabled);
	void Start(bigtime_t time);
	void Stop();

	bool IsStarted();
	
	uint32 OutputChannelCount();

private:
	void ApplyOutputFormat();
	static int32 _mix_thread_(void *arg);
	void MixThread();
	
private:
	BLocker		*fLocker;
	
	BList		*fInputs;
	MixerOutput	*fOutput;
	int32		fNextInputID;
	bool		fRunning;

	Resampler	**fResampler; // array

	float		*fMixBuffer;
	int32		fMixBufferFrameRate;
	int32		fMixBufferFrameCount;
	int32		fMixBufferChannelCount;
	int32		*fMixBufferChannelTypes; //array
	bool		fDoubleRateMixing;
	bigtime_t	fDownstreamLatency;
	
	friend class MixerInput; // XXX debug only
	
	AudioMixer *fNode;
	BBufferGroup *fBufferGroup;
	BTimeSource	*fTimeSource;
	thread_id	fMixThread;
	sem_id		fMixThreadWaitSem;
};


inline void MixerCore::Lock()
{
	fLocker->Lock();
}

inline bool MixerCore::LockWithTimeout(bigtime_t timeout)
{
	return B_OK == fLocker->LockWithTimeout(timeout);
}

inline void MixerCore::Unlock()
{
	fLocker->Unlock();
}

#endif
