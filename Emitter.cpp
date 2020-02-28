#include "Emitter.h"

Emitter::Emitter(int maxParticles, int particlesPerSecond, float lifetime, 
	float startSize, float endSize, XMFLOAT4 startColor, XMFLOAT4 endColor, 
	XMFLOAT3 startVelocity, XMFLOAT3 velocityRandomRange, XMFLOAT3 emitterPosition, 
	XMFLOAT3 positionRandomRange, XMFLOAT4 rotationRandomRanges, XMFLOAT3 emitterAcceleration, 
	ComPtr<ID3D12Device> device,ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12CommandQueue> commandQueue,
	ComPtr<ID3D12PipelineState> particlePipeline,
	ComPtr<ID3D12RootSignature> particleRoot, 
	std::wstring textureName)
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
	//this->texture = texture;

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

	descriptorHeap.Create(device, 3, false, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	indexBuffer = CreateIBView(indices, 6 * maxParticles, device, commandList, defaultIndexHeap, uploadIndexHeap);

	//adding texture and structured buffer to the descriptor heap
	descriptorHeap.CreateDescriptor(textureName, texture, RESOURCE_TYPE_SRV, device, commandQueue, TEXTURE_TYPE_DEAULT);
	//descriptorHeap.CreateStructuredBuffer(particleBuffer, device, maxParticles, sizeof(Particle), sizeof(Particle) * maxParticles);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Particle) * maxParticles),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(particleBuffer.resource.GetAddressOf())
	));

	ZeroMemory(particles, maxParticles * sizeof(Particle));
	particleBuffer.resource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&particleDataBegin));
	memcpy(particleDataBegin, particles, maxParticles * sizeof(Particle));

	//creating the constant buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(externalDataResource.GetAddressOf())
	));

	ZeroMemory(&externData, sizeof(ParticleExternalData));
	externalDataResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&externDataBegin));
	memcpy(externDataBegin, &externData, sizeof(ParticleExternalData));

	delete[] indices;
}

Emitter::~Emitter()
{
	delete[] particles;
}

XMFLOAT3 Emitter::GetPosition()
{
	return emitterPosition;
}

void Emitter::SetPosition(XMFLOAT3 pos)
{
	emitterPosition = pos;
}

void Emitter::SetAcceleration(XMFLOAT3 acel)
{
	emitterAcceleration = acel;
}

void Emitter::UpdateParticles(float deltaTime, float currentTime)
{
	if (isTemp)
	{
		emitterAge += deltaTime;
		if (emitterAge >= emitterLifetime)
		{
			isDead = true;
		}
	}

	if (livingParticleCount > 0)
	{
		//looping through the circular buffer
		if (firstAliveIndex < firstDeadIndex)
		{
			for (int i = firstAliveIndex; i < firstDeadIndex; i++)
			{
				UpdateSingleParticle(deltaTime, i, currentTime);
			}
		}

		//if firse alive is ahead
		else if(firstDeadIndex<firstAliveIndex)
		{
			//go from the first alive to end of list
			for (int i = firstAliveIndex; i < maxParticles; i++)
			{
				UpdateSingleParticle(deltaTime, i, currentTime);
			}

			//go from zero to dead
			for (int i = 0; i < firstDeadIndex; i++)
			{
				UpdateSingleParticle(deltaTime, i, currentTime);
			}
		}

		else
		{
			for (int i = 0; i < maxParticles; i++)
			{
				UpdateSingleParticle(deltaTime, i, currentTime);
			}
		}
	}

	timeSinceEmit += deltaTime;

	while (timeSinceEmit >= secondsPerParticle)
	{
		SpawnParticle(currentTime);
		timeSinceEmit -= secondsPerParticle;
	}
}

void Emitter::Draw(ComPtr<ID3D12GraphicsCommandList> commandList, std::shared_ptr<GPUHeapRingBuffer> ringBuffer,XMFLOAT4X4 view, XMFLOAT4X4 projection, float currentTime)
{
	memcpy(particleDataBegin, particles, sizeof(Particle) * maxParticles);

	//setting the up the buffer
	UINT stride = 0;
	UINT offset = 0;
	//ID3D11Buffer* nullBuffer = nullptr;
	//context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	commandList->IASetIndexBuffer(&indexBuffer);
	commandList->IASetVertexBuffers(0, 1, nullptr);
	//context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);

	//setting the view and projection matrix
	//vs->SetMatrix4x4("view", view);
	externData.view = view;
	//vs->SetMatrix4x4("projection", projection);
	externData.projection = projection;
	//vs->SetFloat3("acceleration", emitterAcceleration);
	externData.acceleration = emitterAcceleration;
	//vs->SetFloat4("startColor", startColor);
	externData.startColor = startColor;
	//vs->SetFloat4("endColor", endColor);
	externData.endColor = endColor;
	//vs->SetFloat("startSize", startSize);
	externData.startSize = startSize;
	//vs->SetFloat("endSize", endSize);
	externData.endSize = endSize;
	//vs->SetFloat("lifetime", lifetime);
	externData.lifetime = lifetime;
	//vs->SetFloat("currentTime", currentTime);
	externData.currentTime = currentTime;

	commandList->SetPipelineState(particlePSO.Get());
	commandList->SetGraphicsRootSignature(particleRootSig.Get());

	commandList->SetGraphicsRootShaderResourceView(0, particleBuffer.resource.Get()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, externalDataResource.Get()->GetGPUVirtualAddress());
	//commandList->SetGraphicsRootConstantBufferView
	commandList->SetGraphicsRootDescriptorTable(2, ringBuffer->GetDescriptorHeap().GetGPUHandle(particleTextureIndex));

	//vs->SetShader();

	//context->VSSetShaderResources(0, 1, &particleData);

	//ps->SetShaderResourceView("particle", texture);
	//ps->SetShader();
	//ps->CopyAllBufferData();

	//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

	if (firstAliveIndex < firstDeadIndex)
	{
		//vs->SetInt("startIndex", firstAliveIndex);
		//vs->CopyAllBufferData();
		externData.startIndex = firstAliveIndex;
		memcpy(externDataBegin, &externData, sizeof(externData));
		//context->DrawIndexed(livingParticleCount * 6, 0, 0);
		commandList->DrawIndexedInstanced(livingParticleCount * 6, 1, 0, 0, 0);
	}

	else
	{
		//draw from 0 to dead
		//vs->SetInt("startIndex", 0);
		//vs->CopyAllBufferData();
		//context->DrawIndexed(firstDeadIndex * 6, 0, 0);
		externData.startIndex = 0;
		memcpy(externDataBegin, &externData, sizeof(externData));
		commandList->DrawIndexedInstanced(firstDeadIndex * 6, 1, 0, 0, 0);

		//draw from alive to max
		//vs->SetInt("startIndex", firstAliveIndex);
		//vs->CopyAllBufferData();
		//context->DrawIndexed((maxParticles - firstAliveIndex) * 6, 0, 0);
		externData.startIndex = firstAliveIndex;
		memcpy(externDataBegin, &externData, sizeof(externData));
		commandList->DrawIndexedInstanced((maxParticles - firstAliveIndex) * 6, 1, 0, 0, 0);
	}
}

void Emitter::SetTemporary(float emitterLife)
{
	isTemp = true;
	this->emitterLifetime = emitterLife;
}

bool Emitter::IsDead()
{
	return isDead;
}

void Emitter::Explosive()
{
	explosive = true;
}

DescriptorHeapWrapper& Emitter::GetDescriptor()
{
	return descriptorHeap;
}

void Emitter::UpdateSingleParticle(float dt, int index, float currentTime)
{
	float age = currentTime - particles[index].spawnTime;

	//if age exceeds its lifespan
	if (age >= lifetime)
	{
		//increase the first alive index
		firstAliveIndex++;
		//wrap it
		firstAliveIndex %= maxParticles;
		//kill this particle
		livingParticleCount--;
		return;
	}
}

void Emitter::SpawnParticle(float currentTime)
{
	if (livingParticleCount == maxParticles)
		return;

	//spawinig a new particle
	particles[firstDeadIndex].spawnTime = currentTime;

	//random position and veloctiy of the particle
	std::random_device rd;
	std::mt19937 randomGenerator(rd());
	std::uniform_real_distribution<float> dist(-1, 1);

	particles[firstDeadIndex].startPosition = emitterPosition; //particles start at emitter
	//randomizing their x,y,z
	particles[firstDeadIndex].startPosition.x += dist(randomGenerator) * positionRandomRange.x;
	particles[firstDeadIndex].startPosition.y += dist(randomGenerator) * positionRandomRange.y;
	particles[firstDeadIndex].startPosition.z += dist(randomGenerator) * positionRandomRange.z;

	particles[firstDeadIndex].startVelocity = startVelocity;
	particles[firstDeadIndex].startVelocity.x += dist(randomGenerator) * velocityRandomRange.x;
	particles[firstDeadIndex].startVelocity.y += dist(randomGenerator) * velocityRandomRange.y;
	particles[firstDeadIndex].startVelocity.z += dist(randomGenerator) * velocityRandomRange.z;

	//random start rotation
	float rotStartMin = rotationRandomRanges.x;
	float rotStartMax = rotationRandomRanges.y;

	//choosing a random start rotation
	particles[firstDeadIndex].rotationStart = dist(randomGenerator) * (rotStartMax - rotStartMin) + rotStartMin;

	//random start rotation
	float rotEndMin = rotationRandomRanges.z;
	float rotEndMax = rotationRandomRanges.w;

	//choosing a random start rotation
	particles[firstDeadIndex].rotationEnd = dist(randomGenerator) * (rotEndMax - rotEndMin) + rotEndMin;

	//increment the first dead index
	firstDeadIndex++;
	firstDeadIndex %= maxParticles;

	//increment living particles
	livingParticleCount++;
}
