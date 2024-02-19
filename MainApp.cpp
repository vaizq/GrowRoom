//
// Created by vaige on 16.2.2024.
//

#include "MainApp.h"
#include "imgui-SFML.h"
#include <SFML/Window/Event.hpp>


static constexpr int fps{144};

MainApp::MainApp()
        : mWindow(sf::VideoMode(640, 480), "Application")
{
    mWindow.setFramerateLimit(fps);
    if (!ImGui::SFML::Init(mWindow))
        throw std::runtime_error("Failed to initialize ImGui");

    // Construct plugins
    mPlugins.push_back(std::make_unique<ReservoirController>());
}

MainApp::~MainApp()
{
    ImGui::SFML::Shutdown();
}

void MainApp::run()
{
    sf::Clock deltaClock;

    while (mWindow.isOpen())
    {
        sf::Event event{};
        while (mWindow.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(mWindow, event);
            if (event.type == sf::Event::Closed) {
                mWindow.close();
            }
        }

        const auto dt = deltaClock.restart();
        ImGui::SFML::Update(mWindow, dt);
        for (auto& plugin : mPlugins) {
            plugin->onGUI();
        }

        mWindow.clear();
        ImGui::SFML::Render(mWindow);
        mWindow.display();
    }
}
