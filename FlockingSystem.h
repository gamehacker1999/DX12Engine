#pragma once

#include<entity\registry.hpp>
#include<DirectXMath.h>
#include<memory>
#include<vector>
#include"Flocker.h"
#include"Entity.h"

class FlockingSystem
{
	static void CalculateFlockCenterAndDirection(std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3& centerPos, XMFLOAT3& direction);
	 
	static XMVECTOR CalcSteeringForces(Flocker& flocker, std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3 centerPos, XMFLOAT3 direction, float deltaTime, UINT flockerID);
	 
	static XMVECTOR Seperation(std::vector<std::shared_ptr<Entity>> flockers, UINT flockerID, Flocker& flocker);
	 
	static XMVECTOR Cohesion(XMFLOAT3 centerPos, Flocker& flocker);
	 
	static XMVECTOR Alignment(XMFLOAT3 direction, Flocker& flocker);
	 
	static XMVECTOR Seek(XMFLOAT3 point, Flocker& flocker);
	 
	static XMVECTOR Flee(XMFLOAT3 point, Flocker& flocker);
	 
	static XMVECTOR StayInPark(Flocker& flocker);

public:
	static void FlockerSystem(entt::registry& registry, std::vector<std::shared_ptr<Entity>> flockers, float deltaTime);

};

//void FlockerSystem(entt::registry& registry, std::vector<std::shared_ptr<Entity>> flockers, float deltaTime);
