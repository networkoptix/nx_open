#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

class QnGraphicsWebView;

class QnWebResourceWidget : public QnResourceWidget
{
    Q_OBJECT

    typedef QnResourceWidget base_type;
public:
    QnWebResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);

    virtual ~QnWebResourceWidget();

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

    virtual QString calculateDetailsText() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

private:
    void setupOverlays();

    virtual int calculateButtonsVisibility() const override;

    virtual void optionsChangedNotify(Options changedFlags) override;

private:
    virtual bool eventFilter(QObject *object
        , QEvent *event) override;

private:
    QnGraphicsWebView * const m_webView;
};
