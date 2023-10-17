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

/**
 * A multi-page content widget implemented in QML for LeftWorkbenchPanel.
 *
 * Its maximum width and fixed height are dictated by current workbench configuration.
 * Its current width is adjusted internally: the QML component contains interactive resizers and
 * a client state save/restore delegate.
 */
class LeftPanelWidget:
    public QQuickWidget,
    public WindowContextAware,
    public HelpTopicQueryable
{
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    using base_type = QQuickWidget;

public:
    LeftPanelWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~LeftPanelWidget() override;

    /** A horizontal position controlled by open/closed panel transitions. */
    qreal position() const;
    void setPosition(qreal value);

    /** Height should be set via this function and not `setGeometry` or `resize`. */
    void setHeight(qreal value);

    /** A maximum width to be set by the workbench. */
    void setMaximumAllowedWidth(qreal value);

    /** Interactive resizers should be disabled in closed panel state. */
    void setResizeEnabled(bool value);

    qreal opacity() const;
    void setOpacity(qreal value);

    menu::ActionScope currentScope() const;
    menu::Parameters currentParameters(menu::ActionScope scope) const;

    QQuickItem* openCloseButton() const;

signals:
    void positionChanged();

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

    using base_type::setGeometry;
    using base_type::resize;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
