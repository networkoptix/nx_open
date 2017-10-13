#pragma once

#include "../event_panel.h"

#include <QtWidgets/QTabWidget>

namespace nx {
namespace client {
namespace desktop {

class NotificationListWidget;
class EventSearchWidget;
class EventListModel;

class EventPanel::Private: public QObject
{
    Q_OBJECT

public:
    Private(EventPanel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    static void makeTranslucent(QWidget* widget);

private:
    EventPanel* q = nullptr;
    QTabWidget* m_tabs = nullptr;
    QnVirtualCameraResourcePtr m_camera;

    NotificationListWidget* m_systemTab = nullptr;
    EventSearchWidget* m_cameraTab = nullptr;

    EventListModel* m_notificationsModel = nullptr;
    EventListModel* m_eventSearchModel = nullptr;

    enum class Tab
    {
        system,
        camera
    };
};

} // namespace
} // namespace client
} // namespace nx
