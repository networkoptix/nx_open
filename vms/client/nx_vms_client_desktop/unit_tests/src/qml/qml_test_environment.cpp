// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_environment.h"

#include <client/client_meta_types.h>
#include <client_core/client_core_meta_types.h>
#include <nx/utils/external_resources.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

using namespace nx::utils;

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const QString kQmlRoot = ":/qml/";

} // namespace

QmlTestEnvironment::QmlTestEnvironment(QObject* parent):
    QObject(parent),
    m_testContext(new Context())
{
    engine()->setBaseUrl(QUrl(kQmlRoot));
    engine()->addImportPath(kQmlRoot);
    core::initializeMetaTypes();
    QnClientMetaTypes::initialize();
    registerExternalResource("client_core_external.dat");
    registerExternalResource("client_external.dat");
}

QmlTestEnvironment::~QmlTestEnvironment()
{
    unregisterExternalResource("client_external.dat");
    unregisterExternalResource("client_core_external.dat");
}

QQmlEngine* QmlTestEnvironment::engine() const
{
    return appContext()->qmlEngine();
}

SystemContext* QmlTestEnvironment::systemContext() const
{
    return m_testContext->systemContext();
}

} // namespace test
} // namespace nx::vms::client::desktop
