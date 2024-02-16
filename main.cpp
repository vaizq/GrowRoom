#include <iostream>
#include "MainApp.h"

int main()
{
    try
    {
        MainApp app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Application failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
