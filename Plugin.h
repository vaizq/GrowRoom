//
// Created by vaige on 31.1.2024.
//

#ifndef TESTPROJECT_APP_H
#define TESTPROJECT_APP_H


#include <chrono>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>



class Plugin {
public:
    using Clock = std::chrono::steady_clock;
    virtual ~Plugin() = default;
    virtual void handleEvents(const sf::Event& event) = 0;
    virtual void update(Clock::duration dt) = 0;
    virtual void render(sf::RenderWindow& window) = 0;
};


#endif //TESTPROJECT_APP_H
