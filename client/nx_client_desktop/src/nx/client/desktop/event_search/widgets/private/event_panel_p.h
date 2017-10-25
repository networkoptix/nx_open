#pragma once

#include "../event_panel.h"

#include <QtWidgets/QTabWidget>

namespace nx {
namespace client {
namespace desktop {

class NotificationListWidget;
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
    EventPanel* q = nullptr;
    QTabWidget* m_tabs = nullptr;

    NotificationListWidget* m_systemTab = nullptr;
    EventSearchWidget* m_cameraTab = nullptr;

    enum class Tab
    {
        system,
        camera
    };
};

} // namespace
} // namespace client
} // namespace nx
