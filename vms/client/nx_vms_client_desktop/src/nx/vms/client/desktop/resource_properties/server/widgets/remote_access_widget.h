// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_properties/server/models/remote_access_model.h>

namespace nx::vms::client::desktop {

class RemoteAccessWidget: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    RemoteAccessWidget(QQmlEngine* engine, QWidget* parent = nullptr);
    void setServer(const ServerResourcePtr& server);

private:
    RemoteAccessModel* m_model;
};

} // namespace nx::vms::client::desktop
