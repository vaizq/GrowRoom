# GrowRoom
GUI for IOT grow room devices, such as [ReservoirController](https://github.com/vaizq/ReservoirController).

# Architecture
GrowRoom's job is to create UI for devices RPC interfaces.
GrowRoom uses ImGui as it's GUI library and a plugin based architecture (see [Plugin](./Plugin.h) and [ReservoirController](./ReservoirController.h) for example).