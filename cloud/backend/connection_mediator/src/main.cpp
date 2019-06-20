#include <QtCore/QCoreApplication>

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/service_main.h>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);

    return nx::utils::runService<nx::hpm::MediatorProcess>(argc, argv);
}
