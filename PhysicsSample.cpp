void CPhysicsSystem::CheckAllCollisions(std::vector<CEntity*>* toCheck)
{
	std::queue<Collision> collidingThisFrame;
	std::forward_list<Collision> endingThisFrame;
	CEntity* pLeft = nullptr, *pRight = nullptr;
	ICollider* pLHS = nullptr, *pRHS = nullptr;
	bool bCollisionFound = true;

	CEntityManager* ObjectManager = CGame::GetInstance()->GetObjectManager();

	
	for (unsigned int nIterations = 0; nIterations < NUM_COLLISION_ITERATIONS && bCollisionFound; nIterations++)
	{
		for (unsigned int i = 0; i < ObjectManager->GetKeyStart(); i++)
		{
			pLeft = toCheck->at(i);

			//Ensure that pLeft is active and has a valid collider
			if (!(pLeft->GetAlive() && pLeft->GetCollider() && pLeft->GetLoaded()))
				continue;

			pLHS = pLeft->GetCollider(); //Do triangle collision and reaction before we proceed
			if (GridToUse == 1)
				pLHS->CheckTriangleCollisions(m_cGridOne.GetTriangles(pLeft->GetTranslation()));
			else if (GridToUse == 2)
				pLHS->CheckTriangleCollisions(m_cGridTwo.GetTriangles(pLeft->GetTranslation()));
			else
				pLHS->CheckTriangleCollisions(m_cGridThree.GetTriangles(pLeft->GetTranslation()));

			//And now for the right hand object
			for (unsigned int j = i + 1; j < toCheck->size(); j++)
			{
				pRight = toCheck->at(j);

				//Ensure that pRight is active and has valid collider
				if (!(pRight->GetAlive() && pRight->GetCollider() && pRight->GetLoaded()))
					continue;

				//Create an std::pair with the entities we're checking
				Collision tTempColl = decltype(tTempColl)(pLeft, pRight);
				pRHS = pRight->GetCollider();

				//Find collision rule that describes said pair
				CollisionRule* pRule = FindRule(pLHS, pRHS);

				//If both colliders are enabled
				if (!(pLHS->GetEnabled() && pRHS->GetEnabled()))
					continue;

				//Finally we can begin checking for collisions
				if (pLHS->CheckCollisions(pRHS, *pRule))
				{
					if (pRule->GetResolveDynamic().first && pRule->GetPush().second && pRule->GetResolveDynamic().second && pRule->GetPush().first)
						bCollisionFound = true;

					collidingThisFrame.push(tTempColl);
				}
				else
				{
					//If this collision was ongoing, end it
					OngoingCollision ongoingMatch = LookupOngoingCollision(tTempColl);

					if (nullptr != ongoingMatch.first)
					{
						Collision newEnding = Collision(ongoingMatch.first, m_OngoingCollisions[ongoingMatch.first][ongoingMatch.second]);
						endingThisFrame.push_front(newEnding);
					}
				}
			}
		}

		//Continue ongoing collisions and add new ones to ongoing collision list
		while (!collidingThisFrame.empty())
		{
			std::pair<CEntity* const, CEntity*> coll = collidingThisFrame.front();
			std::vector<CEntity*>& collisionBucket = m_OngoingCollisions[coll.first];
			bool bCollisionFound = false;

			for (unsigned int i = 0; i < collisionBucket.size(); i++)
			{
				if (coll.second == collisionBucket[i] && (coll.first->GetCollider()->GetOngoingEvents() || coll.second->GetCollider()->GetOngoingEvents()))
				{
					CMessageSystem::GetInstance()->QueueEvent(CGame::GetInstance()->GetHashTags()->nCollisionOngoing, nullptr, coll.first, coll.second);
					bCollisionFound = true;
				}
			}
			if (!bCollisionFound)
			{
				//We have a new collision to add
				CMessageSystem::GetInstance()->QueueEvent(CGame::GetInstance()->GetHashTags()->nCollisionStart, nullptr, nullptr, nullptr, true, coll.first, coll.second);
				m_OngoingCollisions[coll.first].push_back(coll.second);
			}

			collidingThisFrame.pop();
		}

		//Remove collisions that have ended this frame
		while (!endingThisFrame.empty())
		{
			std::pair<CEntity*, CEntity*> endingCollision = endingThisFrame.front();
			OngoingCollision toRemove = LookupOngoingCollision(endingCollision);
			std::vector<CEntity*>& collisionBucket = m_OngoingCollisions[toRemove.first];
			CMessageSystem::GetInstance()->QueueEvent(CGame::GetInstance()->GetHashTags()->nCollisionEnd, nullptr, toRemove.first, collisionBucket[toRemove.second]);

			collisionBucket.erase(collisionBucket.begin() + toRemove.second);

			endingThisFrame.pop_front();
		}

		//Finally process any collision events that have been queued up
		CMessageSystem::GetInstance()->DispatchEvents();
	}
}