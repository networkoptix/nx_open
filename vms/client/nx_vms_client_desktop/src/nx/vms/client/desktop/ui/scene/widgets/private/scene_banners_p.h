// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <utils/common/event_processors.h>

#include "../scene_banners.h"

namespace nx::vms::client::desktop {

class SceneBanners::Private: public QObject
{
    Q_OBJECT

public:
    Private(SceneBanners* q, QWidget* parentWidget):
        q(q),
        container(new QQuickWidget(appContext()->qmlEngine(), parentWidget))
    {
        NX_CRITICAL(parentWidget, "Parent widget must be specified");
        container->setResizeMode(QQuickWidget::SizeViewToRootObject);
        container->setClearColor(Qt::transparent);
        container->setAttribute(Qt::WA_TransparentForMouseEvents);
        container->setAttribute(Qt::WA_TranslucentBackground);
        container->setAttribute(Qt::WA_ShowWithoutActivating);
        container->setAttribute(Qt::WA_AlwaysStackOnTop);
        container->setSource(QUrl("Nx/Items/SceneBanners.qml"));
    }

    SceneBanners* const q;
    const QPointer<QQuickWidget> container;

    void updateMaximumWidth()
    {
        constexpr int kPadding = 64;
        const auto maximumWidth = container->parentWidget()->width() - kPadding * 2;
        container->rootObject()->setProperty("maximumWidth", maximumWidth);
    }

    void initialize()
    {
        connect(container->rootObject(), SIGNAL(removed(QVariant)),
            this, SLOT(handleRemoved(const QVariant&)));

        installEventHandler(container->parentWidget(), QEvent::Resize, q,
            [this]() { updateMaximumWidth(); });

        updateMaximumWidth();
    }

    Q_SLOT void handleRemoved(const QVariant& uuid)
    {
        emit q->removed(uuid.value<QnUuid>());
    }
};

} // namespace nx::vms::client::desktop
