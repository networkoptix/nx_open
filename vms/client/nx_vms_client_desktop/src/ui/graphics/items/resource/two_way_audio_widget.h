#pragma once

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnTwoWayAudioWidgetPrivate;

class QnTwoWayAudioWidget : public Customized<GraphicsWidget>
{
    Q_OBJECT
    Q_PROPERTY(QnTwoWayAudioWidgetColors colors READ colors WRITE setColors)
    typedef Customized<GraphicsWidget> base_type;

public:
    QnTwoWayAudioWidget(const QString& sourceId, QGraphicsWidget* parent = nullptr);
    ~QnTwoWayAudioWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void setFixedHeight(qreal height);

    QnTwoWayAudioWidgetColors colors() const;
    void setColors(const QnTwoWayAudioWidgetColors& value);

signals:
    /* Emitted when internal button is pressed. Mostly for statistics usage. */
    void pressed();

protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QScopedPointer<QnTwoWayAudioWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnTwoWayAudioWidget)
};
