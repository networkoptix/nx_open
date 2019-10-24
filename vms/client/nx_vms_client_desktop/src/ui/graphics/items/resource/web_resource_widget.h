#pragma once

#include <QtWebEngineWidgets/QWebEnginePage>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>

class QnGraphicsWebView;

class QnWebResourceWidget : public QnResourceWidget
{
    Q_OBJECT

    typedef QnResourceWidget base_type;
public:
    QnWebResourceWidget(QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent = NULL);
    virtual ~QnWebResourceWidget();

    virtual bool eventFilter(QObject* object, QEvent* event) override;

    QWebPage* page() const;

    void triggerWebAction(QWebEnginePage::WebAction action);
    bool isWebActionEnabled(QWebEnginePage::WebAction action);
    QString webActionText(QWebEnginePage::WebAction action);

protected:
    virtual int helpTopicAt(const QPointF& pos) const override;

    virtual QString calculateDetailsText() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual Qn::RenderStatus paintChannelBackground(QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect) override;

private:
    void initWebActionText();
    void setupOverlays();

    virtual int calculateButtonsVisibility() const override;

    virtual void optionsChangedNotify(Options changedFlags) override;

protected:
    QnGraphicsWebView* m_webView;
    nx::vms::client::desktop::GraphicsWebEngineView* m_webEngineView;
    QHash<QWebEnginePage::WebAction, QString> m_webActionText;
};
