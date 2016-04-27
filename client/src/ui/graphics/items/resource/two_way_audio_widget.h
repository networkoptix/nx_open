#pragma once

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_widget.h>

#include <utils/common/connective.h>

class QnTwoWayAudioWidgetPrivate;

class QnTwoWayAudioWidget : public Connective<Customized<GraphicsWidget> >
{
    Q_OBJECT
    Q_PROPERTY(QnTwoWayAudioWidgetColors colors READ colors WRITE setColors)
    typedef Connective<Customized<GraphicsWidget> > base_type;

public:
    QnTwoWayAudioWidget(QGraphicsWidget* parent = nullptr);
    ~QnTwoWayAudioWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void setFixedHeight(qreal height);

    QnTwoWayAudioWidgetColors colors() const;
    void setColors(const QnTwoWayAudioWidgetColors& value);
protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QScopedPointer<QnTwoWayAudioWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnTwoWayAudioWidget)
};
