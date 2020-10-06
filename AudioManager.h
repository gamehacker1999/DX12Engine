#pragma once
#include"DX12Helper.h"
#include<Audio.h>
#include<memory>
#include<unordered_map>
#include<vector>
#include<string>
using namespace DirectX;

class SoundEffectManager
{
	std::unique_ptr<DirectX::SoundEffect> soundEffect;
	std::unique_ptr<SoundEffectInstance> effect;
	bool is3D;
	bool isInstanced;
	AudioListener audioListener;
	AudioEmitter audioEmitter;

public:
	SoundEffectManager(std::unique_ptr<AudioEngine>& audioEngine, std::wstring sound,bool isInstance = true,bool is3D = false);
	void Create3DEffect(XMFLOAT3 listener, XMFLOAT3 emitter);

	void PlayEffect(bool isLooping = false);
};

class AudioManager
{
	std::unique_ptr<DirectX::AudioEngine> audioEngine;
	std::vector<std::shared_ptr<SoundEffectManager>> soundEffects;

public:
	AudioManager(bool is3D = false,AUDIO_ENGINE_FLAGS flags = AudioEngine_Default);
	bool Update(float deltaTime = 0);
	std::shared_ptr<SoundEffectManager>AddSoundEffect(std::wstring sound, bool is3D, bool isInstance);
};

