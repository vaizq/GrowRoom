//
// Created by vaige on 31.1.2024.
//

#ifndef TESTPROJECT_APP_H
#define TESTPROJECT_APP_H


#include <chrono>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>


class App {
public:
    using Clock = std::chrono::steady_clock;
    App();
    virtual ~App();
    virtual void run();
    virtual void handleEvents(const sf::Event& event) {}
    virtual void update(Clock::duration dt) {}
    virtual void render() {}
protected:
    sf::RenderWindow mWindow;
};


#endif //TESTPROJECT_APP_H
