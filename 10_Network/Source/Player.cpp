#include <Book/Player.hpp>
#include <Book/CommandQueue.hpp>
#include <Book/Aircraft.hpp>
#include <Book/Foreach.hpp>
#include <Book/NetworkProtocol.hpp>

#include <SFML/Network/Packet.hpp>

#include <map>
#include <string>
#include <algorithm>


using namespace std::placeholders;

struct AircraftMover
{
	AircraftMover(float vx, float vy, int identifier)
	: velocity(vx, vy)
	, aircraftID(identifier)
	{
	}

	void operator() (Aircraft& aircraft, sf::Time) const
	{
		if (aircraft.getIdentifier() == aircraftID)
			aircraft.accelerate(velocity * aircraft.getMaxSpeed());
	}

	sf::Vector2f velocity;
	int aircraftID;
};

struct AircraftFireTrigger
{
	AircraftFireTrigger(int identifier)
	: aircraftID(identifier)
	{
	}

	void operator() (Aircraft& aircraft, sf::Time) const
	{
		if (aircraft.getIdentifier() == aircraftID)
			aircraft.fire();
	}

	int aircraftID;
};

struct AircraftMissileTrigger
{
	AircraftMissileTrigger(int identifier)
	: aircraftID(identifier)
	{
	}

	void operator() (Aircraft& aircraft, sf::Time) const
	{
		if (aircraft.getIdentifier() == aircraftID)
			aircraft.launchMissile();
	}

	int aircraftID;
};


Player::Player(sf::TcpSocket* socket, int identifier, const KeyBinding* binding)
: mKeyBinding(binding)
, mCurrentMissionStatus(MissionRunning)
, mIdentifier(identifier)
, mSocket(socket)
{
	// Set initial action bindings
	initializeActions();

	// Assign all categories to player's aircraft
	FOREACH(auto& pair, mActionBinding)
		pair.second.category = Category::PlayerAircraft;
}

void Player::handleEvent(const sf::Event& event, CommandQueue& commands)
{

	// Event
	if (event.is<sf::Event::KeyPressed>())
	{
		const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
		Action action;
		if (mKeyBinding && keyEvent != nullptr && mKeyBinding->checkAction(keyEvent->code, action) && !isRealtimeAction(action))
		{
			// Network connected -> send event over network
			if (mSocket)
			{
				sf::Packet packet;
				packet << static_cast<int>(Client::PlayerEvent);
				packet << mIdentifier;
				packet << static_cast<int>(action);		
				mSocket->send(packet);
			}

			// Network disconnected -> local event
			else
			{
				commands.push(mActionBinding[action]);
			}
		}
	}

	// Realtime change (network connected)
	if ((event.is<sf::Event::KeyPressed>() || event.is<sf::Event::KeyReleased>()) && mSocket)
	{
		const auto* keyEvent1 = event.getIf<sf::Event::KeyPressed>();
		const auto* keyEvent2 = event.getIf<sf::Event::KeyReleased>();
		sf::Keyboard::Key key_code;
		if(keyEvent1 != nullptr)
			key_code = keyEvent1->code;
		if(keyEvent2 != nullptr)
			key_code = keyEvent2->code;

		Action action;
		if (mKeyBinding 
			&& (keyEvent1 != nullptr || keyEvent2 != nullptr) 
			&& mKeyBinding->checkAction(key_code, action) && isRealtimeAction(action))
		{
			// Send realtime change over network
			sf::Packet packet;
			packet << static_cast<int>(Client::PlayerRealtimeChange);
			packet << mIdentifier;
			packet << static_cast<int>(action);
			packet << (event.is<sf::Event::KeyPressed>());
			mSocket->send(packet);
		}
	}
}

bool Player::isLocal() const
{
	// No key binding means this player is remote
	return mKeyBinding != nullptr;
}

void Player::disableAllRealtimeActions()
{
	FOREACH(auto& action, mActionProxies)
	{
		sf::Packet packet;
		packet << static_cast<int>(Client::PlayerRealtimeChange);
		packet << mIdentifier;
		packet << static_cast<int>(action.first);
		packet << false;
		mSocket->send(packet);
	}
}

void Player::handleRealtimeInput(CommandQueue& commands)
{
	// Check if this is a networked game and local player or just a single player game
	if ((mSocket && isLocal()) || !mSocket)
	{
		// Lookup all actions and push corresponding commands to queue
		std::vector<Action> activeActions = mKeyBinding->getRealtimeActions();
		FOREACH(Action action, activeActions)
			commands.push(mActionBinding[action]);
	}
}

void Player::handleRealtimeNetworkInput(CommandQueue& commands)
{
	if (mSocket && !isLocal())
	{
		// Traverse all realtime input proxies. Because this is a networked game, the input isn't handled directly
		FOREACH(auto pair, mActionProxies)
		{
			if (pair.second && isRealtimeAction(pair.first))
				commands.push(mActionBinding[pair.first]);
		}
	}
}

void Player::handleNetworkEvent(Action action, CommandQueue& commands)
{
	commands.push(mActionBinding[action]);
}

void Player::handleNetworkRealtimeChange(Action action, bool actionEnabled)
{
	mActionProxies[action] = actionEnabled;
}

void Player::setMissionStatus(MissionStatus status)
{
	mCurrentMissionStatus = status;
}

Player::MissionStatus Player::getMissionStatus() const
{
	return mCurrentMissionStatus;
}

void Player::initializeActions()
{
	mActionBinding[PlayerAction::MoveLeft].action      = derivedAction<Aircraft>(AircraftMover(-1,  0, mIdentifier));	
	mActionBinding[PlayerAction::MoveRight].action     = derivedAction<Aircraft>(AircraftMover(+1,  0, mIdentifier));
	mActionBinding[PlayerAction::MoveUp].action        = derivedAction<Aircraft>(AircraftMover( 0, -1, mIdentifier));
	mActionBinding[PlayerAction::MoveDown].action      = derivedAction<Aircraft>(AircraftMover( 0, +1, mIdentifier));
	mActionBinding[PlayerAction::Fire].action          = derivedAction<Aircraft>(AircraftFireTrigger(mIdentifier));
	mActionBinding[PlayerAction::LaunchMissile].action = derivedAction<Aircraft>(AircraftMissileTrigger(mIdentifier));
}
