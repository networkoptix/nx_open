#include "utils.h"

#include <iostream>
#include <string>

namespace nx::cctu {

void waitForExitCommand()
{
    std::cout << std::endl << "Type \"exit\" to close the application" << std::endl;
    for (;;)
    {
        std::string command;
        std::getline(std::cin, command);
        if (command == "exit")
            return;
        else
            std::cout << "Unknown command: " << command << std::endl;
    }
}

} // namespace nx::cctu
