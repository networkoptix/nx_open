#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/updatable.h>

class QnAligner;
namespace Ui
{
    class ImageControlWidget;
}

class QnImageControlWidget : public QWidget, public QnUpdatable
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QWidget base_type;

public:
    QnImageControlWidget(QWidget *parent = nullptr);
    virtual ~QnImageControlWidget();

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
    QScopedPointer<Ui::ImageControlWidget> ui;
    QnAligner* m_aligner = nullptr;
    bool m_readOnly;
};
