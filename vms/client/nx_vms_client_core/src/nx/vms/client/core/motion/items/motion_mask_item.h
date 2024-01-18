// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/motion/motion_grid.h>

namespace nx::vms::client::core {

class MotionMaskItem: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QByteArray motionMask READ motionMask WRITE setMotionMask NOTIFY motionMaskChanged)

    using base_type = QQuickItem;

public:
    explicit MotionMaskItem(QQuickItem* parent = nullptr);
    virtual ~MotionMaskItem() override;

    static constexpr int kMotionMaskSizeBytes = MotionGrid::kCellCount / 8;
    static_assert(MotionGrid::kCellCount % 8 == 0);

    // Unaligned binary motion mask, MotionGrid::kGridSize bits, each byte MSB first.
    QByteArray motionMask() const;
    void setMotionMask(const QByteArray& value);

    virtual bool isTextureProvider() const override;
    virtual QSGTextureProvider* textureProvider() const override;

    static void registerQmlType();

signals:
    void motionMaskChanged();

protected:
    virtual QSGNode* updatePaintNode(
        QSGNode* node, UpdatePaintNodeData* updatePaintNodeData) override;
    virtual void releaseResources() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
