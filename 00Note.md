

### sf::Event 类 依赖于 std::optional std::variant std::holds_alternative std::get_if
- sf::Event 类有 std::variant 成员变量， is getIf 成员方法
    -  // event-> 是 std::optional 的便捷语法等价于 (*event).），仅在 optional 有值时可用，而循环条件已经保证了这一点
    -  // is 内部以  std::holds_alternative 实现， 有模板参数类型的值时返回true， 否则返回false
    -  // std::get_if 是 std::variant 的成员函数， 有模板参数类型的值时返回对应类型的指针，否则返回nullptr

```
void					handlePlayerInput(sf::Keyboard::Key key, bool isPressed);


void Game::processEvents()
{
	while (auto event = mWindow.pollEvent()) {
        // event-> 是 std::optional 的便捷语法等价于 (*event).），仅在 optional 有值时可用，而循环条件已经保证了这一点
        // is 内部以  std::holds_alternative 实现， 有模板参数类型的值时返回true， 否则返回false
		if (event->is<sf::Event::Closed>()) {
			mWindow.close();
		}
        else if (event->is<sf::Event::KeyPressed>()) {
			// std::get_if 是 std::variant 的成员函数， 有模板参数类型的值时返回对应类型的指针，否则返回nullptr
            handlePlayerInput(event->getIf<sf::Event::KeyPressed>()->code, true);
		}
		else if (event->is<sf::Event::KeyReleased>()) {
			handlePlayerInput(event->getIf<sf::Event::KeyReleased>()->code, false);
		}
	}
}
```

- sf::KeyBoard::Key sf::KeyBoard::Scan 是 enmu class
    - sf::KeyBoard::Key 
    - sf::KeyBoard::Scan sf::KeyBoard::Scancode


```
	// getIf 返回的是指针 sf::Event::KeyPressed*   有  code/scancode 等方法
    const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
	if (keyEvent != nullptr &&	keyEvent->code == sf::Keyboard::Key::Enter)
```
