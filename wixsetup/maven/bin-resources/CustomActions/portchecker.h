#include <set>

class PortChecker {
public:
    PortChecker();

    bool isPortAvailable(u_short port);

private:
    int fillBusyPorts();

private:
    std::set<u_short> ports;
};
