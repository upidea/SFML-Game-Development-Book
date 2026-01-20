#include <Book/GameServer.hpp>
#include <Book/NetworkProtocol.hpp>
#include <Book/Foreach.hpp>
#include <Book/Utility.hpp>
#include <Book/Pickup.hpp>
#include <Book/Aircraft.hpp>

#include <SFML/Network/Packet.hpp>

GameServer::RemotePeer::RemotePeer() 
: ready(false)
, timedOut(false)
{
	socket.setBlocking(false);
}

GameServer::GameServer(sf::Vector2f battlefieldSize)
: mThread(&GameServer::executionThread, this)
, mListeningState(false)
, mClientTimeoutTime(sf::seconds(3.f))
, mMaxConnectedPlayers(10)
, mConnectedPlayers(0)
, mWorldHeight(5000.f)
, mBattleFieldRect({0.f, mWorldHeight - battlefieldSize.y}, {battlefieldSize.x, battlefieldSize.y})
, mBattleFieldScrollSpeed(-50.f)
, mAircraftCount(0)
, mPeers(1)
, mAircraftIdentifierCounter(1)
, mWaitingThreadEnd(false)
, mLastSpawnTime(sf::Time::Zero)
, mTimeForNextSpawn(sf::seconds(5.f))
{
	mListenerSocket.setBlocking(false);
	mPeers[0].reset(new RemotePeer());
	// mThread.launch();
}

GameServer::~GameServer()
{
	mWaitingThreadEnd = true;
	// mThread.wait();
	if (mThread.joinable()) {
    	mThread.join();
	}
}

void GameServer::notifyPlayerRealtimeChange(int aircraftIdentifier, int action, bool actionEnabled)
{
	for (std::size_t i = 0; i < mConnectedPlayers; ++i)
	{
		if (mPeers[i]->ready)
		{
			sf::Packet packet;
			packet << static_cast<int>(Server::PlayerRealtimeChange);
			packet << aircraftIdentifier;
			packet << action;
			packet << actionEnabled;

			mPeers[i]->socket.send(packet);
		}
	}
}

void GameServer::notifyPlayerEvent(int aircraftIdentifier, int action)
{
	for (std::size_t i = 0; i < mConnectedPlayers; ++i)
	{
		if (mPeers[i]->ready)
		{
			sf::Packet packet;
			packet << static_cast<int>(Server::PlayerEvent);
			packet << aircraftIdentifier;
			packet << action;

			mPeers[i]->socket.send(packet);
		}
	}
}

void GameServer::notifyPlayerSpawn(int aircraftIdentifier)
{
	for (std::size_t i = 0; i < mConnectedPlayers; ++i)
	{
		if (mPeers[i]->ready)
		{
			sf::Packet packet;
			packet << static_cast<int>(Server::PlayerConnect);
			packet << aircraftIdentifier << mAircraftInfo[aircraftIdentifier].position.x << mAircraftInfo[aircraftIdentifier].position.y;
			mPeers[i]->socket.send(packet);
		}
	}
}

void GameServer::setListening(bool enable)
{
	// Check if it isn't already listening
	if (enable)
	{	
		if (!mListeningState)
			mListeningState = (mListenerSocket.listen(ServerPort) == sf::TcpListener::Status::Done);
	}
	else
	{
		mListenerSocket.close();
		mListeningState = false;
	}
}

void GameServer::executionThread()
{
	setListening(true);

	sf::Time stepInterval = sf::seconds(1.f / 60.f);
	sf::Time stepTime = sf::Time::Zero;
	sf::Time tickInterval = sf::seconds(1.f / 20.f);
	sf::Time tickTime = sf::Time::Zero;
	sf::Clock stepClock, tickClock;

	while (!mWaitingThreadEnd)
	{	
		handleIncomingPackets();
		handleIncomingConnections();

		stepTime += stepClock.getElapsedTime();
		stepClock.restart();

		tickTime += tickClock.getElapsedTime();
		tickClock.restart();

		// Fixed update step
		while (stepTime >= stepInterval)
		{
			mBattleFieldRect.position.y += mBattleFieldScrollSpeed * stepInterval.asSeconds();
			stepTime -= stepInterval;
		}

		// Fixed tick step
		while (tickTime >= tickInterval)
		{
			tick();
			tickTime -= tickInterval;
		}

		// Sleep to prevent server from consuming 100% CPU
		sf::sleep(sf::milliseconds(100));
	}	
}

void GameServer::tick()
{
	updateClientState();

	// Check for mission success = all planes with position.y < offset
	bool allAircraftsDone = true;
	FOREACH(auto pair, mAircraftInfo)
	{
		// As long as one player has not crossed the finish line yet, set variable to false
		if (pair.second.position.y > 0.f)
			allAircraftsDone = false;
	}
	if (allAircraftsDone)
	{
		sf::Packet missionSuccessPacket;
		missionSuccessPacket << static_cast<int>(Server::MissionSuccess);
		sendToAll(missionSuccessPacket);
	}

	// Remove IDs of aircraft that have been destroyed (relevant if a client has two, and loses one)
	for (auto itr = mAircraftInfo.begin(); itr != mAircraftInfo.end(); )
	{
		if (itr->second.hitpoints <= 0)
			mAircraftInfo.erase(itr++);
		else
			++itr;
	}

	// Check if its time to attempt to spawn enemies
	if (now() >= mTimeForNextSpawn + mLastSpawnTime)
	{	
		// No more enemies are spawned near the end
		if (mBattleFieldRect.position.y > 600.f)
		{
			std::size_t enemyCount = 1u + randomInt(2);
			float spawnCenter = static_cast<float>(randomInt(500) - 250);

			// In case only one enemy is being spawned, it appears directly at the spawnCenter
			float planeDistance = 0.f;
			float nextSpawnPosition = spawnCenter;
			
			// In case there are two enemies being spawned together, each is spawned at each side of the spawnCenter, with a minimum distance
			if (enemyCount == 2)
			{
				planeDistance = static_cast<float>(150 + randomInt(250));
				nextSpawnPosition = spawnCenter - planeDistance / 2.f;
			}

			// Send the spawn orders to all clients
			for (std::size_t i = 0; i < enemyCount; ++i)
			{
				sf::Packet packet;
				packet << static_cast<int>(Server::SpawnEnemy);
				packet << static_cast<int>(1 + randomInt(Aircraft::TypeCount-1));
				packet << mWorldHeight - mBattleFieldRect.position.y + 500;
				packet << nextSpawnPosition;

				nextSpawnPosition += planeDistance / 2.f;

				sendToAll(packet);
			}

			mLastSpawnTime = now();
			mTimeForNextSpawn = sf::milliseconds(2000 + randomInt(6000));
		}
	}
}

sf::Time GameServer::now() const
{
	return mClock.getElapsedTime();
}

void GameServer::handleIncomingPackets()
{
	bool detectedTimeout = false;
	
	FOREACH(PeerPtr& peer, mPeers)
	{
		if (peer->ready)
		{
			sf::Packet packet;
			while (peer->socket.receive(packet) == sf::Socket::Status::Done)
			{
				// Interpret packet and react to it
				handleIncomingPacket(packet, *peer, detectedTimeout);

				// Packet was indeed received, update the ping timer
				peer->lastPacketTime = now();
				packet.clear();
			}

			if (now() >= peer->lastPacketTime + mClientTimeoutTime)
			{
				peer->timedOut = true;
				detectedTimeout = true;
			}
		}
	}

	if (detectedTimeout)
		handleDisconnections();
}

void GameServer::handleIncomingPacket(sf::Packet& packet, RemotePeer& receivingPeer, bool& detectedTimeout)
{
	int packetType;
	packet >> packetType;

	switch (packetType)
	{
		case Client::Quit:
		{
			receivingPeer.timedOut = true;
			detectedTimeout = true;
		} break;

		case Client::PlayerEvent:
		{
			int aircraftIdentifier;
			int action;
			packet >> aircraftIdentifier >> action;

			notifyPlayerEvent(aircraftIdentifier, action);
		} break;

		case Client::PlayerRealtimeChange:
		{
			int aircraftIdentifier;
			int action;
			bool actionEnabled;
			packet >> aircraftIdentifier >> action >> actionEnabled;
			mAircraftInfo[aircraftIdentifier].realtimeActions[action] = actionEnabled;
			notifyPlayerRealtimeChange(aircraftIdentifier, action, actionEnabled);
		} break;

		case Client::RequestCoopPartner:
		{
			receivingPeer.aircraftIdentifiers.push_back(mAircraftIdentifierCounter);
			mAircraftInfo[mAircraftIdentifierCounter].position = sf::Vector2f(mBattleFieldRect.size.x / 2, mBattleFieldRect.position.y + mBattleFieldRect.size.y / 2);
			mAircraftInfo[mAircraftIdentifierCounter].hitpoints = 100;
			mAircraftInfo[mAircraftIdentifierCounter].missileAmmo = 2;

			sf::Packet requestPacket;
			requestPacket << static_cast<int>(Server::AcceptCoopPartner);
			requestPacket << mAircraftIdentifierCounter;
			requestPacket << mAircraftInfo[mAircraftIdentifierCounter].position.x;
			requestPacket << mAircraftInfo[mAircraftIdentifierCounter].position.y;

			receivingPeer.socket.send(requestPacket);
			mAircraftCount++;

			// Inform every other peer about this new plane
			FOREACH(PeerPtr& peer, mPeers)
			{
				if (peer.get() != &receivingPeer && peer->ready)
				{
					sf::Packet notifyPacket;
					notifyPacket << static_cast<int>(Server::PlayerConnect);
					notifyPacket << mAircraftIdentifierCounter;
					notifyPacket << mAircraftInfo[mAircraftIdentifierCounter].position.x;
					notifyPacket << mAircraftInfo[mAircraftIdentifierCounter].position.y;
					peer->socket.send(notifyPacket);
				}
			}
			mAircraftIdentifierCounter++;
		} break;

		case Client::PositionUpdate:
		{
			int numAircrafts;
			packet >> numAircrafts;

			for (int i = 0; i < numAircrafts; ++i)
			{
				int aircraftIdentifier;
				int aircraftHitpoints;
				int missileAmmo;
				sf::Vector2f aircraftPosition;
				packet >> aircraftIdentifier >> aircraftPosition.x >> aircraftPosition.y >> aircraftHitpoints >> missileAmmo;
				mAircraftInfo[aircraftIdentifier].position = aircraftPosition;
				mAircraftInfo[aircraftIdentifier].hitpoints = aircraftHitpoints;
				mAircraftInfo[aircraftIdentifier].missileAmmo = missileAmmo;
			}
		} break;

		case Client::GameEvent:
		{
			int action;
			float x;
			float y;

			packet >> action;
			packet >> x;
			packet >> y;

			// Enemy explodes: With certain probability, drop pickup
			// To avoid multiple messages spawning multiple pickups, only listen to first peer (host)
			if (action == GameActions::EnemyExplode && randomInt(3) == 0 && &receivingPeer == mPeers[0].get())
			{
				sf::Packet packet;
				packet << static_cast<int>(Server::SpawnPickup);
				packet << static_cast<int>(randomInt(Pickup::TypeCount));
				packet << x;
				packet << y;

				sendToAll(packet);
			}
		}
	}
}

void GameServer::updateClientState()
{
	sf::Packet updateClientStatePacket;
	updateClientStatePacket << static_cast<int>(Server::UpdateClientState);
	updateClientStatePacket << static_cast<float>(mBattleFieldRect.position.y + mBattleFieldRect.size.y);
	updateClientStatePacket << static_cast<int>(mAircraftInfo.size());

	FOREACH(auto aircraft, mAircraftInfo)
		updateClientStatePacket << aircraft.first << aircraft.second.position.x << aircraft.second.position.y;

	sendToAll(updateClientStatePacket);
}

void GameServer::handleIncomingConnections()
{
	if (!mListeningState)
		return;

	if (mListenerSocket.accept(mPeers[mConnectedPlayers]->socket) == sf::TcpListener::Status::Done)
	{
		// order the new client to spawn its own plane ( player 1 )
		mAircraftInfo[mAircraftIdentifierCounter].position = sf::Vector2f(mBattleFieldRect.size.x / 2, mBattleFieldRect.position.y + mBattleFieldRect.size.y / 2);
		mAircraftInfo[mAircraftIdentifierCounter].hitpoints = 100;
		mAircraftInfo[mAircraftIdentifierCounter].missileAmmo = 2;

		sf::Packet packet;
		packet << static_cast<int>(Server::SpawnSelf);
		packet << mAircraftIdentifierCounter;
		packet << mAircraftInfo[mAircraftIdentifierCounter].position.x;
		packet << mAircraftInfo[mAircraftIdentifierCounter].position.y;
		
		mPeers[mConnectedPlayers]->aircraftIdentifiers.push_back(mAircraftIdentifierCounter);
		
		broadcastMessage("New player!");
		informWorldState(mPeers[mConnectedPlayers]->socket);
		notifyPlayerSpawn(mAircraftIdentifierCounter++);

		mPeers[mConnectedPlayers]->socket.send(packet);
		mPeers[mConnectedPlayers]->ready = true;
		mPeers[mConnectedPlayers]->lastPacketTime = now(); // prevent initial timeouts
		mAircraftCount++;
		mConnectedPlayers++;

		if (mConnectedPlayers >= mMaxConnectedPlayers)
			setListening(false);
		else // Add a new waiting peer
			mPeers.push_back(PeerPtr(new RemotePeer()));
	}
}

void GameServer::handleDisconnections()
{
	for (auto itr = mPeers.begin(); itr != mPeers.end(); )
	{
		if ((*itr)->timedOut)
		{
			// Inform everyone of the disconnection, erase 
			FOREACH(int identifier, (*itr)->aircraftIdentifiers)
			{
				sendToAll(sf::Packet() << static_cast<int>(Server::PlayerDisconnect) << identifier);

				mAircraftInfo.erase(identifier);
			}

			mConnectedPlayers--;
			mAircraftCount -= (*itr)->aircraftIdentifiers.size();

			itr = mPeers.erase(itr);

			// Go back to a listening state if needed
			if (mConnectedPlayers < mMaxConnectedPlayers)
			{
				mPeers.push_back(PeerPtr(new RemotePeer()));
				setListening(true);
			}
				
			broadcastMessage("An ally has disconnected.");
		}
		else
		{
			++itr;
		}
	}
}

// Tell the newly connected peer about how the world is currently
void GameServer::informWorldState(sf::TcpSocket& socket)
{
	sf::Packet packet;
	packet << static_cast<int>(Server::InitialState);
	packet << mWorldHeight << mBattleFieldRect.position.y + mBattleFieldRect.size.y;
	packet << static_cast<int>(mAircraftCount);

	for (std::size_t i = 0; i < mConnectedPlayers; ++i)
	{
		if (mPeers[i]->ready)
		{
			FOREACH(int identifier, mPeers[i]->aircraftIdentifiers)
				packet << identifier << mAircraftInfo[identifier].position.x << mAircraftInfo[identifier].position.y << mAircraftInfo[identifier].hitpoints << mAircraftInfo[identifier].missileAmmo;
		}
	}

	socket.send(packet);
}

void GameServer::broadcastMessage(const std::string& message)
{
	for (std::size_t i = 0; i < mConnectedPlayers; ++i)
	{
		if (mPeers[i]->ready)
		{
			sf::Packet packet;
			packet << static_cast<int>(Server::BroadcastMessage);
			packet << message;

			mPeers[i]->socket.send(packet);
		}	
	}
}

void GameServer::sendToAll(sf::Packet& packet)
{
	FOREACH(PeerPtr& peer, mPeers)
	{
		if (peer->ready)
			peer->socket.send(packet);
	}
}
