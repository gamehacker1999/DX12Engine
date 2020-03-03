#include "Systems.h"

XMVECTOR CalcSteeringForces(Flocker& flocker, std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3 centerPos, XMFLOAT3 direction ,float deltaTime, UINT flockerID)
{
	XMVECTOR ultimateForce=XMVectorSet(0,0,0,0);

	ultimateForce += Seperation(flockers,flockerID,flocker);
	ultimateForce += Cohesion(centerPos,flocker);
	ultimateForce += Alignment(direction,flocker);

	ultimateForce = XMVector3Normalize(ultimateForce);
	ultimateForce *= flocker.maxSpeed;

	auto accelTemp = XMLoadFloat3(&flocker.acceleration);
	accelTemp += ultimateForce / flocker.mass;
	XMStoreFloat3(&flocker.acceleration, accelTemp);
	return ultimateForce;
}

void FlockerSystem(entt::registry& registry, std::vector<std::shared_ptr<Entity>> flockers, float deltaTime)
{

	//CalcSteeringForces(registry, flockers, deltaTime);
	entt::basic_view view = registry.view<Flocker>();

	XMFLOAT3 centerPos;
	XMFLOAT3 direction;

	CalculateFlockCenterAndDirection(flockers, centerPos, direction);

	UINT id = 0;

	for (auto entity : view)
	{
		auto& flocker = view.get<Flocker>(entity);

		CalcSteeringForces(flocker, flockers, centerPos,direction,deltaTime,id); //calculate steering force for the flocker

		//using basic integration to update the velocity of the flocker
		auto velTemp = XMLoadFloat3(&flocker.vel);
		auto posTemp = XMLoadFloat3(&flocker.pos);
		velTemp += XMLoadFloat3(&flocker.acceleration) * deltaTime;
		posTemp += velTemp * deltaTime;
		flocker.acceleration = XMFLOAT3(0, 0, 0);
		XMStoreFloat3(&flocker.vel, velTemp);
		XMStoreFloat3(&flocker.pos, posTemp);

		flockers[id]->SetPosition(flocker.pos);

		id++;
	}

}

void CalculateFlockCenterAndDirection(std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3& centerPos, XMFLOAT3& direction)
{
	XMVECTOR flockCenter = XMVectorSet(0, 0, 0, 0);
	XMVECTOR flockDirection = XMVectorSet(0, 0, 0, 0);

	for (int i = 0; i < flockers.size(); i++)
	{
		auto tempDir = XMLoadFloat3(&flockers[i]->GetForward());
		auto tempPos = XMLoadFloat3(&flockers[i]->GetPosition());
		flockDirection += tempDir;
		flockCenter+=tempPos;
	}

	flockCenter /= flockers.size();
	flockDirection = XMVector3Normalize(flockDirection);

	XMStoreFloat3(&centerPos, flockCenter);
	XMStoreFloat3(&direction, flockDirection);
}

XMVECTOR Seperation(std::vector<std::shared_ptr<Entity>> flockers, UINT flockerID, Flocker& flocker)
{
	XMVECTOR seperationForce = XMVectorSet(0, 0, 0, 0);

	for (int i = 0; i < flockers.size(); i++)
	{
		auto diff = XMLoadFloat3(&flockers[flockerID]->GetPosition()) - XMLoadFloat3(&flockers[i]->GetPosition());
		float distance = sqrtf(XMVector3Dot(diff, diff).m128_f32[0]); //getting the distance between the two flockers

		if (i != flockerID && distance < 7)
		{
			seperationForce += Flee(flockers[i]->GetPosition(),flocker);
		}
	}

	return seperationForce;
}

XMVECTOR Cohesion(XMFLOAT3 centerPos, Flocker& flocker)
{
	return Seek(centerPos,flocker);
}

XMVECTOR Alignment(XMFLOAT3 direction,Flocker& flocker)
{
	auto desiredVelocity = XMLoadFloat3(&direction) * flocker.maxSpeed;
	return desiredVelocity - XMLoadFloat3(&flocker.vel);
}

XMVECTOR Seek(XMFLOAT3 point,Flocker& flocker)
{
	auto desiredVelocity = XMLoadFloat3(&point) - XMLoadFloat3(&flocker.pos);
	desiredVelocity = XMVector3Normalize(desiredVelocity);
	desiredVelocity *= flocker.maxSpeed;

	auto seekingForce = desiredVelocity - XMLoadFloat3(&flocker.vel);

	return seekingForce;
}

XMVECTOR Flee(XMFLOAT3 point, Flocker& flocker)
{
	auto desiredVelocity = (XMLoadFloat3(&point) - XMLoadFloat3(&flocker.pos));
	desiredVelocity *= -1;
	desiredVelocity = XMVector3Normalize(desiredVelocity);
	desiredVelocity *= flocker.maxSpeed;

	auto fleeingForce = desiredVelocity - XMLoadFloat3(&flocker.vel);

	return fleeingForce;
}


