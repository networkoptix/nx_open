// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTemporaryDir>

#include <nx/vms/client/mobile/application_context.h>

class QQmlEngine;

namespace nx::vms::client::mobile::test {

/** Slots are invoked by name from quick_test_main_with_setup(). */
class QmlTestSetup: public QObject
{
    Q_OBJECT

public slots:
    void applicationAvailable();
    void qmlEngineAvailable(QQmlEngine* engine);
    void cleanupTestCase();

private:
    QTemporaryDir m_settingsDir;
    std::unique_ptr<ApplicationContext> m_appContext;
};

} // namespace nx::vms::client::mobile::test
