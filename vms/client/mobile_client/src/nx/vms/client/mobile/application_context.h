// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::mobile {

class SystemContext;
class WindowContext;

class ApplicationContext: public core::ApplicationContext
{
    Q_OBJECT
    using base_type = core::ApplicationContext;

    Q_PROPERTY(bool serverTimeMode
        READ isServerTimeMode
        WRITE setServerTimeMode
        NOTIFY serverTimeModeChanged)

public:
    ApplicationContext(QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    SystemContext* currentSystemContext() const;

    WindowContext* mainWindowContext() const;

    Q_INVOKABLE void closeWindow();

    bool isServerTimeMode() const;
    void setServerTimeMode(bool value);

signals:
    void serverTimeModeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext()
{
    return common::ApplicationContext::instance()->as<ApplicationContext>();
}

} // namespace nx::vms::client::mobile
