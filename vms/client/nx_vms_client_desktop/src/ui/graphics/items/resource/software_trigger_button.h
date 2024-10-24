// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "camera_button.h"

namespace nx::vms::client::desktop {

class SoftwareTriggerButtonPrivate;

class SoftwareTriggerButton: public CameraButton
{
    Q_OBJECT
    using base_type = CameraButton;

public:
    SoftwareTriggerButton(QGraphicsItem* parent = nullptr);
    virtual ~SoftwareTriggerButton();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    Q_DECLARE_PRIVATE(SoftwareTriggerButton)
    QScopedPointer<SoftwareTriggerButtonPrivate> d_ptr;
};

} // namespace nx::vms::client::desktop
