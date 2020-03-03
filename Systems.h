#pragma once

#include<entity\registry.hpp>
#include<DirectXMath.h>
#include<memory>
#include<vector>
#include"Flocker.h"
#include"Entity.h"

void FlockerSystem(entt::registry& registry, std::vector<std::shared_ptr<Entity>> flockers, float deltaTime);

void CalculateFlockCenterAndDirection(std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3& centerPos, XMFLOAT3& direction);

XMVECTOR CalcSteeringForces(Flocker& flocker, std::vector<std::shared_ptr<Entity>> flockers, XMFLOAT3 centerPos, XMFLOAT3 direction, float deltaTime, UINT flockerID);

XMVECTOR Seperation(std::vector<std::shared_ptr<Entity>> flockers, UINT flockerID, Flocker& flocker);

XMVECTOR Cohesion(XMFLOAT3 centerPos, Flocker& flocker);

XMVECTOR Alignment(XMFLOAT3 direction, Flocker& flocker);

XMVECTOR Seek(XMFLOAT3 point, Flocker& flocker);

XMVECTOR Flee(XMFLOAT3 point, Flocker& flocker);
