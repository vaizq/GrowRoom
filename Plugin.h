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
    virtual ~Plugin() = default;
    virtual void onGUI() = 0;
};


#endif //TESTPROJECT_APP_H
