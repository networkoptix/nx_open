#pragma once

#include <QtCore/QVector>
#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>
#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

#include <nx/client/core/resource/camera_motion_helper.h>

namespace nx {
namespace client {

namespace core { class CameraMotionHelper; }

namespace desktop {

class MotionRegionsItem: public QQuickItem
{
    Q_OBJECT
    using base_type = QQuickItem;

    Q_PROPERTY(nx::client::core::CameraMotionHelper* motionHelper
        READ motionHelper WRITE setMotionHelper NOTIFY motionHelperChanged)
    Q_PROPERTY(int channel
        READ channel WRITE setChannel NOTIFY channelChanged)
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

    QVector<QColor> sensitivityColors() const;
    void setSensitivityColors(const QVector<QColor>& value);

    QColor borderColor() const;
    void setBorderColor(const QColor& value);

    QColor labelsColor() const;
    void setLabelsColor(const QColor& value);

    qreal fillOpacity() const;
    void setFillOpacity(qreal value);

    Q_INVOKABLE void userAddRect(int sensitivity, const QRect& rect /*in motion grid coords*/);

    static void registerQmlType();

signals:
    void motionHelperChanged();
    void channelChanged();
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

} // namespace desktop
} // namespace client
} // namespace nx
