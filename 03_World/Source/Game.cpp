#include <Book/Game.hpp>
#include <Book/StringHelpers.hpp>

#include <SFML/Window/Event.hpp>


const sf::Time Game::TimePerFrame = sf::seconds(1.f/60.f);

Game::Game()
: mWindow(sf::VideoMode({640, 480}), "World", sf::Style::Close)
, mWorld(mWindow)
, mFont("Media/Sansation.ttf")
, mStatisticsText(mFont)
, mStatisticsUpdateTime()
, mStatisticsNumFrames(0)
{
	mWindow.setFramerateLimit(60);

	mStatisticsText.setPosition({5.f, 5.f});
	mStatisticsText.setCharacterSize(10);
}

void Game::run()
{
	sf::Clock clock;
	sf::Time timeSinceLastUpdate = sf::Time::Zero;
	while (mWindow.isOpen())
	{
		sf::Time elapsedTime = clock.restart();
		timeSinceLastUpdate += elapsedTime;
		while (timeSinceLastUpdate > TimePerFrame)
		{
			timeSinceLastUpdate -= TimePerFrame;

			processEvents();
			update(TimePerFrame);

		}

		updateStatistics(elapsedTime);
		render();
	}
}

void Game::processEvents()
{
	while (auto event = mWindow.pollEvent()) {
		if (event->is<sf::Event::Closed>()) {
			mWindow.close();
		}
		else if (event->is<sf::Event::KeyPressed>()) {
			handlePlayerInput(event->getIf<sf::Event::KeyPressed>()->code, true);
		}
		else if (event->is<sf::Event::KeyReleased>()) {
			handlePlayerInput(event->getIf<sf::Event::KeyReleased>()->code, false);
		}
	}
}

void Game::update(sf::Time elapsedTime)
{
	mWorld.update(elapsedTime);
}

void Game::render()
{
	mWindow.clear();	
	mWorld.draw();

	mWindow.setView(mWindow.getDefaultView());
	mWindow.draw(mStatisticsText);
	mWindow.display();
}

void Game::updateStatistics(sf::Time elapsedTime)
{
	mStatisticsUpdateTime += elapsedTime;
	mStatisticsNumFrames += 1;

	if (mStatisticsUpdateTime >= sf::seconds(1.0f))
	{
		mStatisticsText.setString(
			"Frames / Second = " + toString(mStatisticsNumFrames) + "\n" +
			"Time / Update = " + toString(mStatisticsUpdateTime.asMicroseconds() / mStatisticsNumFrames) + "us");
							 
		mStatisticsUpdateTime -= sf::seconds(1.0f);
		mStatisticsNumFrames = 0;
	}
}

void Game::handlePlayerInput(sf::Keyboard::Key, bool)
{
}
