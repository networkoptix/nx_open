#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class EventSearchWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventSearchWidget(QWidget* parent = nullptr);
    virtual ~EventSearchWidget() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void setAnalyticsSearchRect(const QRectF& relativeRect);

    bool analyticsSearchByAreaEnabled() const;
    void setAnalyticsSearchByAreaEnabled(bool value);

signals:
    void analyticsSearchByAreaEnabledChanged(bool enabled);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
