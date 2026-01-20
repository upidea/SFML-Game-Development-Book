#include <Book/TitleState.hpp>
#include <Book/Utility.hpp>
#include <Book/ResourceHolder.hpp>

#include <SFML/Graphics/RenderWindow.hpp>


TitleState::TitleState(StateStack& stack, Context context)
: State(stack, context)
, mFont("Media/Sansation.ttf")
, mText(mFont)
, mShowText(true)
, mTextEffectTime(sf::Time::Zero)
, mBackgroundSprite(context.textures->get(Textures::TitleScreen))
{
	mText.setString("Press any key to start");
	centerOrigin(mText);
	mText.setPosition(sf::Vector2f(context.window->getSize() / 2u));
}

void TitleState::draw()
{
	sf::RenderWindow& window = *getContext().window;
	window.draw(mBackgroundSprite);

	if (mShowText)
		window.draw(mText);
}

bool TitleState::update(sf::Time dt)
{
	mTextEffectTime += dt;

	if (mTextEffectTime >= sf::seconds(0.5f))
	{
		mShowText = !mShowText;
		mTextEffectTime = sf::Time::Zero;
	}

	return true;
}

bool TitleState::handleEvent(const sf::Event& event)
{
	// If any key is pressed, trigger the next screen
	const auto* keyEvent = event.getIf<sf::Event::KeyReleased>();
	if (keyEvent != nullptr)
	{
		requestStackPop();
		requestStackPush(States::Menu);
	}

	return true;
}