#include <Book/Game.hpp>
#include <Book/StringHelpers.hpp>

const float Game::PlayerSpeed = 100.f;
const sf::Time Game::TimePerFrame = sf::seconds(1.f/60.f);

Game::Game()
: mWindow(sf::VideoMode({640, 480}), "SFML Application", sf::Style::Close)
, mTexture("Media/Textures/Eagle.png")
, mPlayer(mTexture)
, mFont("Media/Sansation.ttf")
, mStatisticsText(mFont)
, mStatisticsUpdateTime()
, mStatisticsNumFrames(0)
, mIsMovingUp(false)
, mIsMovingDown(false)
, mIsMovingRight(false)
, mIsMovingLeft(false)
{
	mWindow.setFramerateLimit(60);

	// mPlayer.setTexture(mTexture);
	mPlayer.setPosition({100.f, 100.f});
	
	// mStatisticsText.setFont(mFont);
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
			// 按游戏刻更新的内容
			update(TimePerFrame);
		}
		// 按deltaTime更新的内容
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
	sf::Vector2f movement(0.f, 0.f);
	if (mIsMovingUp)
		movement.y -= PlayerSpeed;
	if (mIsMovingDown)
		movement.y += PlayerSpeed;
	if (mIsMovingLeft)
		movement.x -= PlayerSpeed;
	if (mIsMovingRight)
		movement.x += PlayerSpeed;
		
	mPlayer.move(movement * elapsedTime.asSeconds());
}

void Game::render()
{
	mWindow.clear();	
	mWindow.draw(mPlayer);
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

void Game::handlePlayerInput(sf::Keyboard::Key key, bool isPressed)
{	
	if (key == sf::Keyboard::Key::W)
		mIsMovingUp = isPressed;
	else if (key == sf::Keyboard::Key::S)
		mIsMovingDown = isPressed;
	else if (key == sf::Keyboard::Key::A)
		mIsMovingLeft = isPressed;
	else if (key == sf::Keyboard::Key::D)
		mIsMovingRight = isPressed;
}
