//
// Created by vaige on 16.2.2024.
//

#ifndef GROWSTUDIO_MAINAPP_H
#define GROWSTUDIO_MAINAPP_H

#include "ReservoirController.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include "Plugin.h"
#include <vector>

class MainApp
{
public:
    MainApp();
    ~MainApp();
    void run();
private:
    sf::RenderWindow mWindow;
    std::vector<std::unique_ptr<Plugin>> mPlugins;
};


#endif //GROWSTUDIO_MAINAPP_H
