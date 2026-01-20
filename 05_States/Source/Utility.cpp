#include <Book/Utility.hpp>

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>

#include <cmath>


void centerOrigin(sf::Sprite& sprite)
{
	sf::FloatRect bounds = sprite.getLocalBounds();
	sprite.setOrigin({std::floor(bounds.position.x + bounds.size.x / 2.f), std::floor(bounds.position.y + bounds.size.y / 2.f)});
}

void centerOrigin(sf::Text& text)
{
	sf::FloatRect bounds = text.getLocalBounds();
	text.setOrigin({std::floor(bounds.position.x + bounds.size.x / 2.f), std::floor(bounds.position.y + bounds.size.y / 2.f)});
}
