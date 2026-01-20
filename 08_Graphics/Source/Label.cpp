#include <Book/Label.hpp>
#include <Book/Utility.hpp>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>


namespace GUI
{

Label::Label(const std::string& text)
: mFont("Media/Sansation.ttf")
, mText(mFont, text, 16)
{
}

bool Label::isSelectable() const
{
    return false;
}

void Label::handleEvent(const sf::Event&)
{
}

void Label::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
	states.transform *= getTransform();
	target.draw(mText, states);
}

void Label::setText(const std::string& text)
{
	mText.setString(text);
}

}