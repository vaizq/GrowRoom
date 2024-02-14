#include <iostream>
#include "ReservoirController.h"

int main()
{
    try
    {
        ReservoirController rc;
        rc.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Application failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
