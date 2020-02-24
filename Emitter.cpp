#include "Emitter.h"

Emitter::Emitter(int maxParticles, int particlesPerSecond, float lifetime, 
	float startSize, float endSize, XMFLOAT4 startColor, XMFLOAT4 endColor, 
	XMFLOAT3 startVelocity, XMFLOAT3 velocityRandomRange, XMFLOAT3 emitterPosition, 
	XMFLOAT3 positionRandomRange, XMFLOAT4 rotationRandomRanges, XMFLOAT3 emitterAcceleration, 
	ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12PipelineState> particlePipeline, ComPtr<ID3D12RootSignature> particleRoot, 
	ManagedResource texture)
{
	this->maxParticles = maxParticles; //max particles spewed
	this->particlesPerSecond = particlesPerSecond; //particles spewed per second
	this->secondsPerParticle = 1.0f / particlesPerSecond; //amount after which a particle is spawned
	this->lifetime = lifetime; //lifetime of each particle
	this->startSize = startSize; //start size
	this->endSize = endSize; //end size
	this->startColor = startColor; //start color to interpolate from
	this->endColor = endColor; //end color to interpolate to
	this->startVelocity = startVelocity; //start velocity
	this->velocityRandomRange = velocityRandomRange; //range of velocity
	this->emitterPosition = emitterPosition; //position of emitter
	this->positionRandomRange = positionRandomRange; //range of pos
	this->rotationRandomRanges = rotationRandomRanges; //random ranges of rotation
	this->emitterAcceleration = emitterAcceleration; //acceleration of emmiter
	this->particlePSO = particlePipeline;
	this->particleRootSig = particleRoot;
	this->texture = texture;

	timeSinceEmit = 0;//how long since the last particle was emmited
	livingParticleCount = 0; //count of how many particles
	//circular buffer indices
	firstAliveIndex = 0;
	firstDeadIndex = 0;
	isDead = false;
	isTemp = false;
	emitterAge = true;
	explosive = false;

	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	//creating the index buffer view
	unsigned int* indices = new unsigned int[6 * maxParticles];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;

	}

	indexBuffer = CreateIBView(indices, 6 * maxParticles, device, commandList, defaultIndexHeap, uploadIndexHeap);
}

Emitter::~Emitter()
{
	delete[] particles;
}
