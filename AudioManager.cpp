#include "AudioManager.h"

AudioManager::AudioManager(bool is3D, AUDIO_ENGINE_FLAGS flags)
{
#ifdef _DEBUG
	flags = AudioEngine_Debug;
#endif // _DEBUG
	audioEngine = std::make_unique<AudioEngine>(flags);

}

bool AudioManager::Update(float deltaTime)
{
	bool isUpdated = audioEngine->Update();

	if (!isUpdated)
	{
		if (audioEngine->IsCriticalError())
		{

		}
	}

	return isUpdated;
}

std::shared_ptr<SoundEffectManager> AudioManager::AddSoundEffect(std::wstring sound, bool is3D, bool isInstance)
{
	std::shared_ptr<SoundEffectManager>soundEffect=std::make_shared<SoundEffectManager>(this->audioEngine, sound, isInstance, is3D);
	soundEffects.emplace_back(soundEffect);
	return soundEffect;
}

SoundEffectManager::SoundEffectManager(std::unique_ptr<AudioEngine>& audioEngine, std::wstring sound, bool isInstance, bool is3D)
{
	soundEffect = std::make_unique<SoundEffect>(audioEngine.get(),sound.c_str());
	this->is3D = is3D;
	this->isInstanced = isInstance;

	SOUND_EFFECT_INSTANCE_FLAGS effectFlags = SoundEffectInstance_Default;

	if (is3D)
	{
		effectFlags = SoundEffectInstance_Use3D
			| SoundEffectInstance_ReverbUseFilters;
	}

	if (isInstance)
	{
		effect = soundEffect->CreateInstance(effectFlags);
	}
}

void SoundEffectManager::Create3DEffect(XMFLOAT3 listener, XMFLOAT3 emitter)
{
	//AudioListener audiolistener;
	audioListener.SetPosition(listener);

	//AudioEmitter audioEmitter;
	audioEmitter.SetPosition(emitter);

	if (is3D)
	{
		this->effect->Apply3D(audioListener, audioEmitter, false);
	}

}

void SoundEffectManager::PlayEffect(bool isLooping)
{
	if (!is3D || !isInstanced)
	{
		isLooping = false;
	}

	if (!isInstanced)
	{
		soundEffect->Play();
		return;
	}

	effect->Play(isLooping);

}
