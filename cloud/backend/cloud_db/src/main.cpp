#include <QtCore/QCoreApplication>

#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_global.h>
#include <nx/utils/service_main.h>

#include <nx/cloud/db/cloud_db_service.h>

int main(int argc, char* argv[])
{
    // NOTE: QCoreApplication is required for loading SQL plugins on mswin.
    QCoreApplication app(argc, argv);

    nx::network::SocketGlobals::InitGuard sgGuard(
        nx::network::InitializationFlags::disableUdt);
    return nx::utils::runService<nx::cloud::db::CloudDbService>(argc, argv);
}
