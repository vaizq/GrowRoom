cmake_minimum_required(VERSION 3.27)
project(GrowStudio)

set(CMAKE_CXX_STANDARD 20)

find_package(SFML 2.5 COMPONENTS graphics audio REQUIRED)
find_package(ImGui-SFML REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(fmt REQUIRED)
find_package(PahoMqttCpp CONFIG REQUIRED)

add_executable(GrowStudio main.cpp
        App.cpp
        MqttClient.h
)

target_link_libraries(GrowStudio PRIVATE sfml-graphics ImGui-SFML::ImGui-SFML fmt::fmt)
target_link_libraries(GrowStudio PRIVATE asio::asio)
target_link_libraries(GrowStudio PRIVATE cereal::cereal)
target_link_libraries(GrowStudio PRIVATE PahoMqttC::PahoMqttC PahoMqttCpp::paho-mqttpp3-static)