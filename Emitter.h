#pragma once
#include "DX12Helper.h"
#include "DescriptorHeapWrapper.h"
#include"GPUHeapRingBuffer.h"
#include"Particles.h"

using namespace DirectX;
//class to have particle emitters

struct ParticleExternalData
{
	Matrix view;
	Matrix projection;

	int startIndex;
	Vector3 acceleration;

	Vector4 startColor;
	Vector4 endColor;

	float startSize;
	float endSize;

	float lifetime;
	float currentTime;
};

class Emitter
{
public:
	Emitter(
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize,
		float endSize,
		Vector4 startColor,
		Vector4 endColor,
		Vector3 startVelocity,
		Vector3 velocityRandomRange,
		Vector3 emitterPosition,
		Vector3 positionRandomRange,
		Vector4 rotationRandomRanges,
		Vector3 emitterAcceleration,
		ComPtr<ID3D12Device> device,
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12CommandQueue> commandQueue,
		ComPtr<ID3D12PipelineState> particlePipeline,
		ComPtr<ID3D12RootSignature> particleRoot,
		std::wstring textureName
	);
	~Emitter();

	Vector3 GetPosition();
	void SetPosition(Vector3 pos);
	void SetAcceleration(Vector3 acel);

	void UpdateParticles(float deltaTime, float currentTime);
	void Draw(ComPtr<ID3D12GraphicsCommandList> commandList,std::shared_ptr<GPUHeapRingBuffer> ringBuffer , Matrix view, Matrix projection, float currentTime);

	void SetTemporary(float emitterLife);
	bool IsDead();
	void Explosive();

	DescriptorHeapWrapper& GetDescriptor();

	UINT particleTextureIndex;

private:
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceEmit;
	bool isTemp; //is the emitter temporary
	float emitterLifetime;
	float emitterAge;
	bool isDead;
	bool explosive;

	int livingParticleCount;
	float lifetime;

	Vector3 emitterAcceleration;
	Vector3 emitterPosition;
	Vector3 startVelocity;

	Vector3 positionRandomRange;
	Vector3 velocityRandomRange;
	Vector4 rotationRandomRanges; // Min start, max start, min end, max end

	Vector4 startColor;
	Vector4 endColor;
	float startSize;
	float endSize;

	// Particle array
	Particle* particles;
	int maxParticles;
	int firstDeadIndex;
	int firstAliveIndex;

	// Rendering
	//ParticleVertex* particleVertices;
	//ID3D11Buffer* vertexBuffer;
	//ID3D11Buffer* indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW vertexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBuffer;

	ComPtr<ID3D12Resource> defaultHeap;
	ComPtr<ID3D12Resource> uploadHeap;

	ComPtr<ID3D12Resource> defaultIndexHeap;
	ComPtr<ID3D12Resource> uploadIndexHeap;

	ManagedResource texture;
	ComPtr<ID3D12PipelineState> particlePSO;
	ComPtr<ID3D12RootSignature> particleRootSig;

	//ID3D11Buffer* particleBuffer;
	ManagedResource particleBuffer;
	ManagedResource particleUploadBuffer;
	UINT8* particleDataBegin;
	ManagedResource particleData;

	//constant buffer data
	ParticleExternalData externData;
	UINT8* externDataBegin;
	ComPtr<ID3D12Resource> externalDataResource;

	//descriptor heap
	DescriptorHeapWrapper descriptorHeap;

	// Update Methods
	void UpdateSingleParticle(float dt, int index, float currentTime);
	void SpawnParticle(float currentTime);
};

