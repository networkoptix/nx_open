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
signals:
    void changed();
    void fisheyeChanged();

private:
    QScopedPointer<Ui::ImageControlWidget> ui;
};
