// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::mobile {

class SystemContext;

class ApplicationContext: public core::ApplicationContext
{
    Q_OBJECT
    using base_type = core::ApplicationContext;

public:
    ApplicationContext(QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    SystemContext* currentSystemContext() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext()
{
    return common::ApplicationContext::instance()->as<ApplicationContext>();
}

} // namespace nx::vms::client::mobile
