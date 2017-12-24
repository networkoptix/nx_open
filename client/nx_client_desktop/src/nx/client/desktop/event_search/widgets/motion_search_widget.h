#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class MotionSearchWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    MotionSearchWidget(QWidget* parent = nullptr);
    virtual ~MotionSearchWidget() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
