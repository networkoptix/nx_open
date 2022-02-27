// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_environment.h"

#include <client_core/client_core_module.h>
#include <client/client_meta_types.h>
#include <common/static_common_module.h>

#include <nx/vms/utils/external_resources.h>

using namespace nx::vms::utils;

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const QString kQmlRoot = ":/qml/";

} // namespace

QmlTestEnvironment::QmlTestEnvironment(QObject* parent):
    QObject(parent),
    m_staticCommonModule(new QnStaticCommonModule()),
    m_clientCoreModule(new QnClientCoreModule(QnClientCoreModule::Mode::unitTests))
{
    engine()->setBaseUrl(QUrl(kQmlRoot));
    engine()->addImportPath(kQmlRoot);
    QnClientMetaTypes::initialize();
    registerExternalResource("client_external.dat");
}

QmlTestEnvironment::~QmlTestEnvironment()
{
    unregisterExternalResource("client_external.dat");
}

QQmlEngine* QmlTestEnvironment::engine() const
{
    return m_clientCoreModule->mainQmlEngine();
}

} // namespace test
} // namespace nx::vms::client::desktop
