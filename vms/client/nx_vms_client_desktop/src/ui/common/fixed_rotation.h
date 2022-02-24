// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsRotation>

#include <nx/vms/client/core/utils/rotation.h>

class QGraphicsWidget;

class QnFixedRotationTransform: public QGraphicsRotation
{
    Q_OBJECT

    typedef QGraphicsRotation base_type;

public:
    QnFixedRotationTransform(QObject *parent = nullptr);

    using base_type::setAngle;
    void setAngle(nx::vms::client::core::Rotation angle);

    QGraphicsWidget *target();
    void setTarget(QGraphicsWidget *target);

private slots:
    void updateOrigin();

private:
    QPointer<QGraphicsWidget> m_target;
};
