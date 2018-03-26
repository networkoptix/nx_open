#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/updatable.h>

class QnAligner;

namespace Ui { class LegacyImageControlWidget; }

namespace nx {
namespace client {
namespace desktop {

class LegacyImageControlWidget: public QWidget, public QnUpdatable
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    LegacyImageControlWidget(QWidget *parent = nullptr);
    virtual ~LegacyImageControlWidget();

    QnAligner* aligner() const;

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

private:
    void updateAspectRatioFromResources(const QnVirtualCameraResourceList &cameras);
    void updateRotationFromResources(const QnVirtualCameraResourceList &cameras);

signals:
    void changed();

private:
    QScopedPointer<Ui::LegacyImageControlWidget> ui;
    QnAligner* m_aligner = nullptr;
    bool m_readOnly;
};

} // namespace desktop
} // namespace client
} // namespace nx
