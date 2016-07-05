#include "util.h"

#include <iostream>

int main()
{
    std::cout << getDataDirectory().toLatin1().data() << std::endl;
    return 0;
}
