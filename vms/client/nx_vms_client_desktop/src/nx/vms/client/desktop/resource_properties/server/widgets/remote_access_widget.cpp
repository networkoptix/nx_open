// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_access_widget.h"

#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/client/desktop/utils/qml_property.h>

namespace nx::vms::client::desktop {

RemoteAccessWidget::RemoteAccessWidget(QQmlEngine* engine, QWidget* parent):
    base_type(engine, parent), m_model(new RemoteAccessModel(this))
{
    setClearColor(palette().window().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl("Nx/Dialogs/ServerSettings/RemoteAccess.qml"));

    if (!NX_ASSERT(rootObject()))
        return;

    rootObject()->setProperty("model", QVariant::fromValue(m_model));
}

void RemoteAccessWidget::setServer(const ServerResourcePtr& server)
{
    NX_ASSERT(server);
    m_model->setServer(server);
}

} // namespace nx::vms::client::desktop
