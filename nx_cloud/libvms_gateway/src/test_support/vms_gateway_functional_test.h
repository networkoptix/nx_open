/**********************************************************
* May 19, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include "vms_gateway_process_public.h"


namespace nx {
namespace cloud {
namespace gateway {

class VmsGatewayFunctionalTest
:
    public utils::test::ModuleLauncher<VmsGatewayProcessPublic>
{
public:
    VmsGatewayFunctionalTest();
    ~VmsGatewayFunctionalTest();

private:
    QString m_tmpDir;
    int m_httpPort;
};

}   // namespace gateway
}   // namespace cloud
}   // namespace nx
