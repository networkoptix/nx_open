#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <utils/common/updatable.h>

namespace Ui { class LegacyImageControlWidget; }

namespace nx::vms::client::desktop {

class Aligner;

class LegacyImageControlWidget: public QWidget, public QnUpdatable
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    LegacyImageControlWidget(QWidget *parent = nullptr);
    virtual ~LegacyImageControlWidget();

    Aligner* aligner() const;

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
    Aligner* m_aligner = nullptr;
    bool m_readOnly;
};

} // namespace nx::vms::client::desktop
