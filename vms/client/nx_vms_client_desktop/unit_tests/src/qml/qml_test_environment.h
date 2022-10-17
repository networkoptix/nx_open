// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop::test {

class Context;

class QmlTestEnvironment: public QObject
{
public:
    explicit QmlTestEnvironment(QObject* parent = nullptr);
    virtual ~QmlTestEnvironment() override;

    QQmlEngine* engine() const;

    SystemContext* systemContext() const;

private:
    std::unique_ptr<Context> m_testContext;
};

} // namespace nx::vms::client::desktop::test
