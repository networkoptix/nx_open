// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/action_types.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/common/help_topic_queryable.h>

namespace nx::vms::client::desktop {

class QmlResourceBrowserWidget:
    public QQuickWidget,
    public WindowContextAware,
    public HelpTopicQueryable
{
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    using base_type = QQuickWidget;

public:
    QmlResourceBrowserWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~QmlResourceBrowserWidget() override;

    /** A horizontal position controlled by open/closed panel transitions. */
    qreal position() const;
    void setPosition(qreal value);

    qreal opacity() const;
    void setOpacity(qreal value);

    menu::Parameters currentParameters(menu::ActionScope scope) const;

signals:
    void positionChanged();

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
