#include <Book/TextNode.hpp>
#include <Book/Utility.hpp>

#include <SFML/Graphics/RenderTarget.hpp>

    
TextNode::TextNode(const std::string& text):mFont("Media/Sansation.ttf"), mText(mFont)
{
	mText.setCharacterSize(20);
	setString(text);
}

void TextNode::drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
	target.draw(mText, states);
}

void TextNode::setString(const std::string& text)
{
	mText.setString(text);
	centerOrigin(mText);
}