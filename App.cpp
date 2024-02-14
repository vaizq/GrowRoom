//
// Created by vaige on 31.1.2024.
//

#include "App.h"
#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Window/Event.hpp>

constexpr int fps = 144;

App::App()
: mWindow(sf::VideoMode(640, 480), "Application")
{
    mWindow.setFramerateLimit(fps);
    if (!ImGui::SFML::Init(mWindow))
        throw std::runtime_error("Failed to initialize ImGui");
}

App::~App() {
    ImGui::SFML::Shutdown();
}

void App::run() {

    sf::Clock deltaClock;

    while (mWindow.isOpen())
    {
        sf::Event event{};
        while (mWindow.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(mWindow, event);
            handleEvents(event);

            if (event.type == sf::Event::Closed) {
                mWindow.close();
            }
        }

        const auto dt = deltaClock.restart();
        ImGui::SFML::Update(mWindow, dt);
        update(std::chrono::microseconds(dt.asMicroseconds()));

        mWindow.clear();
        render();
        ImGui::SFML::Render(mWindow);
        mWindow.display();
    }
}
