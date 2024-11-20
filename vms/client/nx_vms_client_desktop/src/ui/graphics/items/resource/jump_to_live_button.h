// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include "camera_button.h"

namespace nx::vms::client::desktop {

class JumpToLiveButtonPrivate;

class JumpToLiveButton: public CameraButton
{
    Q_OBJECT
    using base_type = CameraButton;

public:
    JumpToLiveButton(
        QGraphicsItem* parent = nullptr,
        const std::optional<QColor>& pressedColor = {},
        const std::optional<QColor>& activeColor = {},
        const std::optional<QColor>& normalColor = {});

    virtual ~JumpToLiveButton();

    QString toolTip() const;
    virtual void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    Q_DECLARE_PRIVATE(JumpToLiveButton)
    QScopedPointer<JumpToLiveButtonPrivate> d_ptr;
};

} // namespace nx::vms::client::desktop
