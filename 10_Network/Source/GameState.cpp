#include <Book/GameState.hpp>
#include <Book/MusicPlayer.hpp>

#include <SFML/Graphics/RenderWindow.hpp>


GameState::GameState(StateStack& stack, Context context)
: State(stack, context)
, mWorld(*context.window, *context.sounds, false)
, mPlayer(nullptr, 1, context.keys1)
{
	mWorld.addAircraft(1);
	mPlayer.setMissionStatus(Player::MissionRunning);

	// Play game theme
	context.music->play(Music::MissionTheme);
}

void GameState::draw()
{
	mWorld.draw();
}

bool GameState::update(sf::Time dt)
{
	mWorld.update(dt);

	if (!mWorld.hasAlivePlayer())
	{
		mPlayer.setMissionStatus(Player::MissionFailure);
		requestStackPush(States::GameOver);
	}
	else if (mWorld.hasPlayerReachedEnd())
	{
		mPlayer.setMissionStatus(Player::MissionSuccess);
		requestStackPush(States::MissionSuccess);
	}

	CommandQueue& commands = mWorld.getCommandQueue();
	mPlayer.handleRealtimeInput(commands);

	return true;
}

bool GameState::handleEvent(const sf::Event& event)
{
	// Game input handling
	CommandQueue& commands = mWorld.getCommandQueue();

	const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
	if (keyEvent != nullptr)
		mPlayer.handleEvent(*keyEvent, commands);

	// Escape pressed, trigger the pause screen
	if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Escape)
		requestStackPush(States::Pause);

	return true;
}
