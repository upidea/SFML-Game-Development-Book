#include <Book/MultiplayerGameState.hpp>
#include <Book/MusicPlayer.hpp>
#include <Book/Foreach.hpp>
#include <Book/Utility.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Network/IpAddress.hpp>

#include <fstream>


sf::IpAddress getAddressFromFile()
{
	{ // Try to open existing file (RAII block)
		std::ifstream inputFile("ip.txt");
		std::string ipAddress;
		if (inputFile >> ipAddress) {
          	std::optional<sf::IpAddress> resolvedIp = sf::IpAddress::resolve(ipAddress);
            if (resolvedIp.has_value()) {
                return resolvedIp.value();
            }
		}
	}

	// If open/read failed, create new file
	std::ofstream outputFile("ip.txt");
	std::string localAddress = "127.0.0.1";
	outputFile << localAddress;
	std::optional<sf::IpAddress> defaultIp = sf::IpAddress::resolve(localAddress);	
	return defaultIp.value();
}

MultiplayerGameState::MultiplayerGameState(StateStack& stack, Context context, bool isHost)
: State(stack, context)
, mWorld(*context.window, *context.sounds, true)
, mFont("Media/Sansation.ttf")
, mPlayerInvitationText(mFont)
, mFailedConnectionText(mFont)
, mBroadcastText(mFont)
, mWindow(*context.window)
, mTextureHolder(*context.textures)
, mConnected(false)
, mGameServer(nullptr)
, mActiveState(true)
, mHasFocus(true)
, mHost(isHost)
, mGameStarted(false)
, mClientTimeout(sf::seconds(2.f))
, mTimeSinceLastPacket(sf::seconds(0.f))
{
	// mBroadcastText.setFont(context.fonts->get(Fonts::Main));
	mBroadcastText.setPosition({1024.f / 2, 100.f});

	// mPlayerInvitationText.setFont(context.fonts->get(Fonts::Main));
	mPlayerInvitationText.setCharacterSize(20);
	mPlayerInvitationText.setFillColor(sf::Color::White);
	mPlayerInvitationText.setString("Press Enter to spawn player 2");
	mPlayerInvitationText.setPosition({1000 - mPlayerInvitationText.getLocalBounds().size.x, 760 - mPlayerInvitationText.getLocalBounds().size.y});

	// We reuse this text for "Attempt to connect" and "Failed to connect" messages
	// mFailedConnectionText.setFont(context.fonts->get(Fonts::Main));
	mFailedConnectionText.setString("Attempting to connect...");
	mFailedConnectionText.setCharacterSize(35);
	mFailedConnectionText.setFillColor(sf::Color::White);
	centerOrigin(mFailedConnectionText);
	mFailedConnectionText.setPosition({mWindow.getSize().x / 2.f, mWindow.getSize().y / 2.f});

	// Render a "establishing connection" frame for user feedback
	mWindow.clear(sf::Color::Black);
	mWindow.draw(mFailedConnectionText);
	mWindow.display();
	mFailedConnectionText.setString("Could not connect to the remote server!");
	centerOrigin(mFailedConnectionText);

	sf::IpAddress ip = isHost ? sf::IpAddress::resolve("127.0.0.1").value()  : getAddressFromFile();

	if (isHost)
	{
		mGameServer.reset(new GameServer(sf::Vector2f(mWindow.getSize())));
	}
	
	if (mSocket.connect(ip, ServerPort, sf::seconds(5.f)) == sf::TcpSocket::Status::Done)
		mConnected = true;
	else
		mFailedConnectionClock.restart();

	mSocket.setBlocking(false);

	// Play game theme
	context.music->play(Music::MissionTheme);
}

void MultiplayerGameState::draw()
{
	if (mConnected)
	{
		mWorld.draw();

		// Broadcast messages in default view
		mWindow.setView(mWindow.getDefaultView());

		if (!mBroadcasts.empty())
			mWindow.draw(mBroadcastText);

		if (mLocalPlayerIdentifiers.size() < 2 && mPlayerInvitationTime < sf::seconds(0.5f))
			mWindow.draw(mPlayerInvitationText);
	}
	else
	{
		mWindow.draw(mFailedConnectionText);
	}
}

void MultiplayerGameState::onActivate()
{
	mActiveState = true;
}

void MultiplayerGameState::onDestroy()
{
	if (!mHost && mConnected)
	{
		// Inform server this client is dying
		sf::Packet packet;
		packet << static_cast<int>(Client::Quit);
		mSocket.send(packet);
	}
}

bool MultiplayerGameState::update(sf::Time dt)
{
	// Connected to server: Handle all the network logic
	if (mConnected)
	{
		mWorld.update(dt);

		// Remove players whose aircrafts were destroyed
		bool foundLocalPlane = false;
		for (auto itr = mPlayers.begin(); itr != mPlayers.end(); )
		{
			// Check if there are no more local planes for remote clients
			if (std::find(mLocalPlayerIdentifiers.begin(), mLocalPlayerIdentifiers.end(), itr->first) != mLocalPlayerIdentifiers.end())
			{
				foundLocalPlane = true;
			}

			if (!mWorld.getAircraft(itr->first))
			{
				itr = mPlayers.erase(itr);

				// No more players left: Mission failed
				if (mPlayers.empty())
					requestStackPush(States::GameOver);
			}
			else
			{
				++itr;
			}
		}

		if (!foundLocalPlane && mGameStarted)
		{
			requestStackPush(States::GameOver);
		}

		// Only handle the realtime input if the window has focus and the game is unpaused
		if (mActiveState && mHasFocus)
		{
			CommandQueue& commands = mWorld.getCommandQueue();
			FOREACH(auto& pair, mPlayers)
				pair.second->handleRealtimeInput(commands);
		}

		// Always handle the network input
		CommandQueue& commands = mWorld.getCommandQueue();
		FOREACH(auto& pair, mPlayers)
			pair.second->handleRealtimeNetworkInput(commands);

		// Handle messages from server that may have arrived
		sf::Packet packet;
		if (mSocket.receive(packet) == sf::Socket::Status::Done)
		{
			mTimeSinceLastPacket = sf::seconds(0.f);
			int packetType;	
			packet >> packetType;
			handlePacket(packetType, packet);	
		}
		else
		{
			// Check for timeout with the server
			if (mTimeSinceLastPacket > mClientTimeout)
			{
				mConnected = false;

				mFailedConnectionText.setString("Lost connection to server");
				centerOrigin(mFailedConnectionText);

				mFailedConnectionClock.restart();
			}
		}

		updateBroadcastMessage(dt);

		// Time counter for blinking 2nd player text
		mPlayerInvitationTime += dt;
		if (mPlayerInvitationTime > sf::seconds(1.f))
			mPlayerInvitationTime = sf::Time::Zero;

		// Events occurring in the game
		GameActions::Action gameAction;
		while (mWorld.pollGameAction(gameAction))
		{
			sf::Packet packet;
			packet << static_cast<int>(Client::GameEvent);
			packet << static_cast<int>(gameAction.type);
			packet << gameAction.position.x;
			packet << gameAction.position.y;

			mSocket.send(packet);
		}

		// Regular position updates
		if (mTickClock.getElapsedTime() > sf::seconds(1.f / 20.f))
		{
			sf::Packet positionUpdatePacket;
			positionUpdatePacket << static_cast<int>(Client::PositionUpdate);
			positionUpdatePacket << static_cast<int>(mLocalPlayerIdentifiers.size());
			
			FOREACH(int identifier, mLocalPlayerIdentifiers)
			{			
				if (Aircraft* aircraft = mWorld.getAircraft(identifier))
					positionUpdatePacket << identifier << aircraft->getPosition().x << aircraft->getPosition().y << static_cast<int>(aircraft->getHitpoints()) << static_cast<int>(aircraft->getMissileAmmo());
			}

			mSocket.send(positionUpdatePacket);
			mTickClock.restart();
		}

		mTimeSinceLastPacket += dt;
	}

	// Failed to connect and waited for more than 5 seconds: Back to menu
	else if (mFailedConnectionClock.getElapsedTime() >= sf::seconds(5.f))
	{
		requestStateClear();
		requestStackPush(States::Menu);
	}

	return true;
}

void MultiplayerGameState::disableAllRealtimeActions()
{
	mActiveState = false;

	FOREACH(int identifier, mLocalPlayerIdentifiers)
		mPlayers[identifier]->disableAllRealtimeActions();
}

bool MultiplayerGameState::handleEvent(const sf::Event& event)
{
	// Game input handling
	CommandQueue& commands = mWorld.getCommandQueue();

	// Forward event to all players
	FOREACH(auto& pair, mPlayers)
		pair.second->handleEvent(event, commands);	


	const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
	if (keyEvent != nullptr)
	{
		// Enter pressed, add second player co-op (only if we are one player)
		if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Enter && mLocalPlayerIdentifiers.size() == 1)
		{
			sf::Packet packet;
			packet << static_cast<int>(Client::RequestCoopPartner);

			mSocket.send(packet);
		}

		// Escape pressed, trigger the pause screen
		else if (keyEvent->code == sf::Keyboard::Key::Escape)
		{
			disableAllRealtimeActions();
			requestStackPush(States::NetworkPause);
		}
	}
	else if (event.is<sf::Event::FocusGained>())
	{
		mHasFocus = true;
	}
	else if (event.is<sf::Event::FocusLost>())
	{
		mHasFocus = false;
	}

	return true;
}

void MultiplayerGameState::updateBroadcastMessage(sf::Time elapsedTime)
{
	if (mBroadcasts.empty())
		return;

	// Update broadcast timer
	mBroadcastElapsedTime += elapsedTime;
	if (mBroadcastElapsedTime > sf::seconds(2.5f))
	{
		// If message has expired, remove it
		mBroadcasts.erase(mBroadcasts.begin());

		// Continue to display next broadcast message
		if (!mBroadcasts.empty())
		{
			mBroadcastText.setString(mBroadcasts.front());
			centerOrigin(mBroadcastText);
			mBroadcastElapsedTime = sf::Time::Zero;
		}
	}
}

void MultiplayerGameState::handlePacket(int packetType, sf::Packet& packet)
{
	switch (packetType)
	{
		// Send message to all clients
		case Server::BroadcastMessage:
		{
			std::string message;
			packet >> message;
			mBroadcasts.push_back(message);	

			// Just added first message, display immediately
			if (mBroadcasts.size() == 1)
			{
				mBroadcastText.setString(mBroadcasts.front());
				centerOrigin(mBroadcastText);
				mBroadcastElapsedTime = sf::Time::Zero;
			}
		} break;

		// Sent by the server to order to spawn player 1 airplane on connect
		case Server::SpawnSelf:
		{
			int aircraftIdentifier;
			sf::Vector2f aircraftPosition;
			packet >> aircraftIdentifier >> aircraftPosition.x >> aircraftPosition.y;

			Aircraft* aircraft = mWorld.addAircraft(aircraftIdentifier);
			aircraft->setPosition(aircraftPosition);
			
			mPlayers[aircraftIdentifier].reset(new Player(&mSocket, aircraftIdentifier, getContext().keys1));
			mLocalPlayerIdentifiers.push_back(aircraftIdentifier);

			mGameStarted = true;
		} break;

		// 
		case Server::PlayerConnect:
		{
			int aircraftIdentifier;
			sf::Vector2f aircraftPosition;
			packet >> aircraftIdentifier >> aircraftPosition.x >> aircraftPosition.y;

			Aircraft* aircraft = mWorld.addAircraft(aircraftIdentifier);
			aircraft->setPosition(aircraftPosition);

			mPlayers[aircraftIdentifier].reset(new Player(&mSocket, aircraftIdentifier, nullptr));
		} break;

		// 
		case Server::PlayerDisconnect:
		{
			int aircraftIdentifier;
			packet >> aircraftIdentifier;

			mWorld.removeAircraft(aircraftIdentifier);
			mPlayers.erase(aircraftIdentifier);
		} break;

		// 
		case Server::InitialState:
		{
			int aircraftCount;
			float worldHeight, currentScroll;
			packet >> worldHeight >> currentScroll;

			mWorld.setWorldHeight(worldHeight);
			mWorld.setCurrentBattleFieldPosition(currentScroll);

			packet >> aircraftCount;
			for (int i = 0; i < aircraftCount; ++i)
			{
				int aircraftIdentifier;
				int hitpoints;
				int missileAmmo;
				sf::Vector2f aircraftPosition;
				packet >> aircraftIdentifier >> aircraftPosition.x >> aircraftPosition.y >> hitpoints >> missileAmmo;

				Aircraft* aircraft = mWorld.addAircraft(aircraftIdentifier);
				aircraft->setPosition(aircraftPosition);
				aircraft->setHitpoints(hitpoints);
				aircraft->setMissileAmmo(missileAmmo);

				mPlayers[aircraftIdentifier].reset(new Player(&mSocket, aircraftIdentifier, nullptr));
			}
		} break;

		//
		case Server::AcceptCoopPartner:
		{
			int aircraftIdentifier;
			packet >> aircraftIdentifier;

			mWorld.addAircraft(aircraftIdentifier);
			mPlayers[aircraftIdentifier].reset(new Player(&mSocket, aircraftIdentifier, getContext().keys2));
			mLocalPlayerIdentifiers.push_back(aircraftIdentifier);
		} break;

		// Player event (like missile fired) occurs
		case Server::PlayerEvent:
		{
			int aircraftIdentifier;
			int action;
			packet >> aircraftIdentifier >> action;

			auto itr = mPlayers.find(aircraftIdentifier);
			if (itr != mPlayers.end())
				itr->second->handleNetworkEvent(static_cast<Player::Action>(action), mWorld.getCommandQueue());
		} break;

		// Player's movement or fire keyboard state changes
		case Server::PlayerRealtimeChange:
		{
			int aircraftIdentifier;
			int action;
			bool actionEnabled;
			packet >> aircraftIdentifier >> action >> actionEnabled;

			auto itr = mPlayers.find(aircraftIdentifier);
			if (itr != mPlayers.end())
				itr->second->handleNetworkRealtimeChange(static_cast<Player::Action>(action), actionEnabled);
		} break;

		// New enemy to be created
		case Server::SpawnEnemy:
		{
			float height;
			int type;
			float relativeX;
			packet >> type >> height >> relativeX;

			mWorld.addEnemy(static_cast<Aircraft::Type>(type), relativeX, height);
			mWorld.sortEnemies();
		} break;

		// Mission successfully completed
		case Server::MissionSuccess:
		{
			requestStackPush(States::MissionSuccess);
		} break;

		// Pickup created
		case Server::SpawnPickup:
		{
			int type;
			sf::Vector2f position;
			packet >> type >> position.x >> position.y;

			mWorld.createPickup(position, static_cast<Pickup::Type>(type));
		} break;

		//
		case Server::UpdateClientState:
		{
			float currentWorldPosition;
			int aircraftCount;
			packet >> currentWorldPosition >> aircraftCount;

			float currentViewPosition = mWorld.getViewBounds().position.y + mWorld.getViewBounds().size.y;

			// Set the world's scroll compensation according to whether the view is behind or too advanced
			mWorld.setWorldScrollCompensation(currentViewPosition / currentWorldPosition);

			for (int i = 0; i < aircraftCount; ++i)
			{
				sf::Vector2f aircraftPosition;
				int aircraftIdentifier;
				packet >> aircraftIdentifier >> aircraftPosition.x >> aircraftPosition.y;

				Aircraft* aircraft = mWorld.getAircraft(aircraftIdentifier);
				bool isLocalPlane = std::find(mLocalPlayerIdentifiers.begin(), mLocalPlayerIdentifiers.end(), aircraftIdentifier) != mLocalPlayerIdentifiers.end();
				if (aircraft && !isLocalPlane)
				{
					sf::Vector2f interpolatedPosition = aircraft->getPosition() + (aircraftPosition - aircraft->getPosition()) * 0.1f;
					aircraft->setPosition(interpolatedPosition);
				}
			}
		} break;
	}
}
