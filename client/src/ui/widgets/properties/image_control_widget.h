#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class ImageControlWidget;
}

class QnImageControlWidget : public QWidget {
    Q_OBJECT

    typedef QWidget base_type;
public:
    QnImageControlWidget(QWidget* parent = 0);
    virtual ~QnImageControlWidget();

    void updateFromResources(const QnVirtualCameraResourceList &cameras);
    void submitToResources(const QnVirtualCameraResourceList &cameras);

    bool isFisheye() const;

private:
    void updateAspectRatioFromResources(const QnVirtualCameraResourceList &cameras);
    void updateRotationFromResources(const QnVirtualCameraResourceList &cameras);
    void updateFisheyeFromResources(const QnVirtualCameraResourceList &cameras);

signals:
    void changed();
    void fisheyeChanged();

private:
    QScopedPointer<Ui::ImageControlWidget> ui;
};
