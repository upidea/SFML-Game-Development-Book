#include <Book/MenuState.hpp>
#include <Book/Utility.hpp>
#include <Book/Foreach.hpp>
#include <Book/ResourceHolder.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/Graphics/RenderStates.hpp>


MenuState::MenuState(StateStack& stack, Context context)
: State(stack, context)
, mFont("Media/Sansation.ttf")
, mOptions()
, mOptionIndex(0)
{
	sf::Texture& texture = context.textures->get(Textures::TitleScreen);
	// sf::Font& font = context.fonts->get(Fonts::Main);

	// mBackgroundSprite.setTexture(texture);
	mBackgroundSprite.emplace(texture);	// emplace 直接在 optional 中构造 Sprite
	
	// A simple menu demonstration
	sf::Text playOption(mFont);
	// playOption.setFont(font);
	playOption.setString("Play");
	centerOrigin(playOption);
	playOption.setPosition(context.window->getView().getSize() / 2.f);
	mOptions.push_back(playOption);

	sf::Text exitOption(mFont);
	// exitOption.setFont(font);
	exitOption.setString("Exit");
	centerOrigin(exitOption);
	exitOption.setPosition(playOption.getPosition() + sf::Vector2f(0.f, 30.f));
	mOptions.push_back(exitOption);

	updateOptionText();
}

void MenuState::draw()
{
	sf::RenderWindow& window = *getContext().window;

	window.setView(window.getDefaultView());
	// 确保 Sprite 已初始化
	if (mBackgroundSprite) {
		window.draw(*mBackgroundSprite);
	}

	FOREACH(const sf::Text& text, mOptions)
		window.draw(text);
}

bool MenuState::update(sf::Time)
{
	return true;
}

bool MenuState::handleEvent(const sf::Event& event)
{
	// The demonstration menu logic
	// if (event.type != sf::Event::KeyPressed)
	// 	return false;

	const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
	if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Enter)
	{
		if (mOptionIndex == Play)
		{
			requestStackPop();
			requestStackPush(States::Game);
		}
		else if (mOptionIndex == Exit)
		{
			// The exit option was chosen, by removing itself, the stack will be empty, and the game will know it is time to close.
			requestStackPop();
		}
	}

	else if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Up)
	{
		// Decrement and wrap-around
		if (mOptionIndex > 0)
			mOptionIndex--;
		else
			mOptionIndex = mOptions.size() - 1;

		updateOptionText();
	}

	else if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Down)
	{
		// Increment and wrap-around
		if (mOptionIndex < mOptions.size() - 1)
			mOptionIndex++;
		else
			mOptionIndex = 0;

		updateOptionText();
	}

	return true;
}

void MenuState::updateOptionText()
{
	if (mOptions.empty())
		return;

	// White all texts
	FOREACH(sf::Text& text, mOptions)
		text.setFillColor(sf::Color::White);

	// Red the selected text
	mOptions[mOptionIndex].setFillColor(sf::Color::Red);
}
