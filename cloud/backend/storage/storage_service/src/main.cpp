#include <QtCore/QCoreApplication>

#include <nx/cloud/storage/service/storage_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/service_main.h>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);
    return nx::utils::runService<nx::cloud::storage::service::StorageService>(argc, argv);
}
