#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <client/client_color_types.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/animation/animated.h>
#include <ui/customization/customized.h>

#include <nx/utils/singleton.h>

class GraphicsLabel;

class QnGraphicsMessageBoxHolder:
    public GraphicsWidget,
    public Singleton<QnGraphicsMessageBoxHolder>
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    explicit QnGraphicsMessageBoxHolder(QGraphicsItem *parent = NULL);
    virtual ~QnGraphicsMessageBoxHolder();
    virtual QRectF boundingRect() const override;

    void addItem(QGraphicsLayoutItem* item);
    void removeItem(QGraphicsLayoutItem* item);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QGraphicsLinearLayout *m_layout;
};


class QnGraphicsMessageBox: public Customized<Animated<QnFramedWidget>>
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QnGraphicsMessageBoxColors colors READ colors WRITE setColors)

    using base_type = Customized<Animated<QnFramedWidget>>;

public:
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = nullptr,
        const QString &text = QString(), int timeoutMsec = 0);
    ~QnGraphicsMessageBox();

    QString text() const;
    void setText(const QString &text);

    QnGraphicsMessageBoxColors colors() const;
    void setColors(const QnGraphicsMessageBoxColors &value);

    int timeout() const;

    static QnGraphicsMessageBox* information(const QString &text, int timeoutMsec = 0);

public:
    void showAnimated();
    void hideAnimated();
    void hideImmideately();

signals:
    void finished();

private:
    void at_animationIn_finished();

private:
    GraphicsLabel *m_label;
    int m_timeout;
    QnGraphicsMessageBoxColors m_colors;
};
