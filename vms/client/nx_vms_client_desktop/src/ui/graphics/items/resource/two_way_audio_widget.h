// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <qt_graphics_items/graphics_widget.h>

#include <nx/utils/impl_ptr.h>

class QnImageButtonWidget;

class QnTwoWayAudioWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    QnTwoWayAudioWidget(
        const QString& sourceId,
        const QnSecurityCamResourcePtr& camera,
        QGraphicsWidget* parent = nullptr);
    virtual ~QnTwoWayAudioWidget() override;

    void setFixedHeight(qreal height);

    QnImageButtonWidget *button() const;

signals:
    void pressed();
    void released();

protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
        override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};
