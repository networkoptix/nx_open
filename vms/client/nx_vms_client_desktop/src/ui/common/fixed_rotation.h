#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsRotation>

#include <nx/vms/client/desktop/common/data/rotation.h>

class QGraphicsWidget;

class QnFixedRotationTransform: public QGraphicsRotation
{
    Q_OBJECT

    typedef QGraphicsRotation base_type;

public:
    QnFixedRotationTransform(QObject *parent = NULL);

    using base_type::setAngle;
    void setAngle(nx::vms::client::desktop::Rotation angle);

    QGraphicsWidget *target();
    void setTarget(QGraphicsWidget *target);

private slots:
    void updateOrigin();

private:
    QPointer<QGraphicsWidget> m_target;
};
