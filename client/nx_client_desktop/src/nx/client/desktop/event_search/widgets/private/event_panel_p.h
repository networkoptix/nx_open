#pragma once

#include "../event_panel.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

#include <nx/utils/disconnect_helper.h>

class QTabWidget;
class QStackedWidget;
class QnMediaResourceWidget;

namespace nx {
namespace client {
namespace desktop {

class NotificationListWidget;
class MotionSearchWidget;
class EventSearchWidget;

class EventPanel::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void paintBackground();

private:
    void currentWorkbenchWidgetChanged(Qn::ItemRole role);

private:
    EventPanel* q = nullptr;
    QTabWidget* m_tabs = nullptr;

    NotificationListWidget* m_systemTab = nullptr;
    QStackedWidget* m_cameraTab = nullptr;
    EventSearchWidget* m_eventsWidget = nullptr;
    MotionSearchWidget* m_motionWidget = nullptr;

    QPointer<QnMediaResourceWidget> m_currentMediaWidget;
    QScopedPointer<QnDisconnectHelper> m_mediaWidgetConnections;

    enum class Tab
    {
        system,
        camera
    };
};

} // namespace
} // namespace client
} // namespace nx
