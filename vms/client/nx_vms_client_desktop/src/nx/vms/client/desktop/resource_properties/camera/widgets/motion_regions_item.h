// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QScopedPointer>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

Q_MOC_INCLUDE("nx/vms/client/core/motion/helpers/camera_motion_helper.h")

namespace nx::vms::client::core { class CameraMotionHelper; }

namespace nx::vms::client::desktop {

class MotionRegionsItem: public QQuickItem
{
    Q_OBJECT
    using base_type = QQuickItem;

    Q_PROPERTY(nx::vms::client::core::CameraMotionHelper* motionHelper
        READ motionHelper WRITE setMotionHelper NOTIFY motionHelperChanged)
    Q_PROPERTY(int channel
        READ channel WRITE setChannel NOTIFY channelChanged)
    Q_PROPERTY(int rotationQuadrants
        READ rotationQuadrants WRITE setRotationQuadrants NOTIFY rotationQuadrantsChanged)
    Q_PROPERTY(QVector<QColor> sensitivityColors
        READ sensitivityColors WRITE setSensitivityColors NOTIFY sensitivityColorsChanged);
    Q_PROPERTY(QColor borderColor
        READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(QColor labelsColor
        READ labelsColor WRITE setLabelsColor NOTIFY labelsColorChanged)
    Q_PROPERTY(qreal fillOpacity
        READ fillOpacity WRITE setFillOpacity NOTIFY fillOpacityChanged)

public:
    MotionRegionsItem(QQuickItem* parent = nullptr);
    virtual ~MotionRegionsItem() override;

    core::CameraMotionHelper* motionHelper() const;
    void setMotionHelper(core::CameraMotionHelper* value);

    int channel() const;
    void setChannel(int value);

    int rotationQuadrants() const;
    void setRotationQuadrants(int value);

    QVector<QColor> sensitivityColors() const;
    void setSensitivityColors(const QVector<QColor>& value);

    QColor borderColor() const;
    void setBorderColor(const QColor& value);

    QColor labelsColor() const;
    void setLabelsColor(const QColor& value);

    qreal fillOpacity() const;
    void setFillOpacity(qreal value);

    static void registerQmlType();

signals:
    void motionHelperChanged();
    void channelChanged();
    void rotationQuadrantsChanged();
    void sensitivityColorsChanged();
    void borderColorChanged();
    void labelsColorChanged();
    void fillOpacityChanged();

protected:
    virtual QSGNode* updatePaintNode(
        QSGNode* node, UpdatePaintNodeData* updatePaintNodeData) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
