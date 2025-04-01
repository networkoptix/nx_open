// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_test_environment.h"

#include <client/client_meta_types.h>
#include <client/client_startup_parameters.h>
#include <nx/utils/external_resources.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

using namespace nx::utils;

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const QString kQmlRoot = ":/qml/";

static const ApplicationContext::Features kFeatures =
    []
    {
        ApplicationContext::Features result = ApplicationContext::Features::none();
        result.core.flags.setFlag(core::ApplicationContext::FeatureFlag::qml);
        return result;
    }();

} // namespace

QmlTestEnvironment::QmlTestEnvironment(QObject* parent):
    QObject(parent),
    m_appContext(new ApplicationContext(
        ApplicationContext::Mode::unitTests,
        kFeatures,
        QnStartupParameters()))
{
    engine()->setBaseUrl(QUrl(kQmlRoot));
    engine()->addImportPath(kQmlRoot);
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
    return m_appContext->qmlEngine();
}

SystemContext* QmlTestEnvironment::systemContext() const
{
    return m_appContext->currentSystemContext();
}

} // namespace test
} // namespace nx::vms::client::desktop
