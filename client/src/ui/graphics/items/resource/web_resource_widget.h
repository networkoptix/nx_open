#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

class QnWebView;

class QnWebResourceWidget : public QnResourceWidget
{
    Q_OBJECT

    typedef QnResourceWidget base_type;
public:
    QnWebResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnWebResourceWidget();

protected:
    virtual QString calculateDetailsText() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

private:
    void setupOverlays();

    int calculateButtonsVisibility();

    void optionsChangedNotify(Options changedFlags);

private:
    QnWebView * const m_webView;
};
