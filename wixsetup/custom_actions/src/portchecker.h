#include <set>

class PortChecker
{
public:
    PortChecker();

    bool isPortAvailable(unsigned short port);

private:
    int fillBusyPorts();

private:
    std::set<unsigned short> ports;
};
