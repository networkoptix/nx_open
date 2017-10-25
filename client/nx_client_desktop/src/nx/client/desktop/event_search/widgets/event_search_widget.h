#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/common/panel.h>

namespace nx {
namespace client {
namespace desktop {

class EventSearchWidget: public QnPanel
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    EventSearchWidget(QWidget* parent = nullptr);
    virtual ~EventSearchWidget() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
