// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

class QnClientCoreModule;
class QnStaticCommonModule;

namespace nx::vms::client::desktop {
namespace test {

class QmlTestEnvironment: public QObject
{
public:
    explicit QmlTestEnvironment(QObject* parent = nullptr);
    virtual ~QmlTestEnvironment() override;

    QQmlEngine* engine() const;

private:
    std::unique_ptr<QnStaticCommonModule> m_staticCommonModule;
    std::unique_ptr<QnClientCoreModule> m_clientCoreModule;
};

} // namespace test
} // namespace nx::vms::client::desktop
