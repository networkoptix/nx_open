#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/common/updatable.h>

namespace Ui {
    class ImageControlWidget;
}

class QnImageControlWidget : public QWidget, public QnUpdatable {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QWidget base_type;
public:
    QnImageControlWidget(QWidget* parent = 0);
    virtual ~QnImageControlWidget();

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

    bool isFisheye() const;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);
private:
    void updateAspectRatioFromResources(const QnVirtualCameraResourceList &cameras);
    void updateRotationFromResources(const QnVirtualCameraResourceList &cameras);
    void updateFisheyeFromResources(const QnVirtualCameraResourceList &cameras);

signals:
    void changed();
    void fisheyeChanged();

private:
    QScopedPointer<Ui::ImageControlWidget> ui;
    bool m_readOnly;
};
