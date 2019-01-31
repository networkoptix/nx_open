#pragma once

#include "basic_component_test.h"

namespace nx::cloud::relay::test {

class MaintenanceServiceBaseTypeImpl
{
public:
    MaintenanceServiceBaseTypeImpl();

    void addArg(const char* arg);

    bool startAndWaitUntilStarted();

    nx::network::SocketAddress httpEndpoint() const;

    QString testDataDir() const;

private:
    std::vector<const char *> argsAsConstCharPtr();

private:
    BasicComponentTest m_test;
    std::vector<std::string> m_args;
};

} // namespace nx::cloud::relay::test

