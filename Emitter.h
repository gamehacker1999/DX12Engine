#pragma once
#include "DX12Helper.h"
#include"Particles.h"
using namespace DirectX;
//class to have particle emitters
class Emitter
{
public:
	Emitter(
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize,
		float endSize,
		XMFLOAT4 startColor,
		XMFLOAT4 endColor,
		XMFLOAT3 startVelocity,
		XMFLOAT3 velocityRandomRange,
		XMFLOAT3 emitterPosition,
		XMFLOAT3 positionRandomRange,
		XMFLOAT4 rotationRandomRanges,
		XMFLOAT3 emitterAcceleration,
		ComPtr<ID3D12Device> device,
		ComPtr<ID3D12GraphicsCommandList> commandList,
		ComPtr<ID3D12PipelineState> particlePipeline,
		ComPtr<ID3D12RootSignature> particleRoot,
		ManagedResource texture
	);
	~Emitter();

	XMFLOAT3 GetPosition();
	void SetPosition(XMFLOAT3 pos);
	void SetAcceleration(XMFLOAT3 acel);

	void UpdateParticles(float deltaTime, float currentTime);
	void Draw(ComPtr<ID3D12CommandList> context, XMFLOAT4X4 view, XMFLOAT4X4 projection, float currentTime);

	void SetTemporary(float emitterLife);
	bool IsDead();
	void Explosive();

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

	XMFLOAT3 emitterAcceleration;
	XMFLOAT3 emitterPosition;
	XMFLOAT3 startVelocity;

	XMFLOAT3 positionRandomRange;
	XMFLOAT3 velocityRandomRange;
	XMFLOAT4 rotationRandomRanges; // Min start, max start, min end, max end

	XMFLOAT4 startColor;
	XMFLOAT4 endColor;
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
	ManagedResource particleData;

	// Update Methods
	void UpdateSingleParticle(float dt, int index, float currentTime);
	void SpawnParticle(float currentTime);
};

