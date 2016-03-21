#pragma once

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <client/client_color_types.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/animation/animated.h>
#include <ui/customization/customized.h>

class GraphicsLabel;

// TODO: #Elric rename, not standard item => no "graphics" prefix?

class QnGraphicsMessageBoxItem: public GraphicsWidget {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    explicit QnGraphicsMessageBoxItem(QGraphicsItem *parent = NULL);
    virtual ~QnGraphicsMessageBoxItem();
    virtual QRectF boundingRect() const override;

    void addItem(QGraphicsLayoutItem* item);
    void removeItem(QGraphicsLayoutItem* item);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QGraphicsLinearLayout *m_layout;
};


class QnGraphicsMessageBox : public Customized<Animated<QnFramedWidget>> {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QnGraphicsMessageBoxColors colors READ colors WRITE setColors)

    typedef Customized<Animated<QnFramedWidget>> base_type;

public:
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = NULL, const QString &text = QString(), int timeoutMsec = 0, int fontSize = 0);
    ~QnGraphicsMessageBox();

    QString text() const;
    void setText(const QString &text);

    QnGraphicsMessageBoxColors colors() const;
    void setColors(const QnGraphicsMessageBoxColors &value);

    int timeout() const;

    static QnGraphicsMessageBox* information(const QString &text, int timeoutMsec = 0, int fontSize = 0);
    static QnGraphicsMessageBox* informationTicking(const QString &text, int timeoutMsec = 0, int fontSize = 0);

public slots:
    void hideImmideately();

signals:
    void finished();
    void tick(int time);

private slots:
    void at_animationIn_finished();

private:
    GraphicsLabel *m_label;
    int m_timeout;
    QnGraphicsMessageBoxColors m_colors;
};
