#ifndef GRAPHICS_MESSAGE_BOX_H
#define GRAPHICS_MESSAGE_BOX_H

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/animation/animated.h>

#include "framed_widget.h"

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


class QnGraphicsMessageBox : public Animated<QnFramedWidget> {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    typedef Animated<QnFramedWidget> base_type;

public:
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = NULL, const QString &text = QString(), int timeoutMsec = 0, int fontSize = 0);
    ~QnGraphicsMessageBox();

    QString text() const;
    void setText(const QString &text);

    const QColor &textColor() const;
    void setTextColor(const QColor &textColor);

    int timeout() const;

    static QnGraphicsMessageBox* information(const QString &text, int timeoutMsec = 0, int fontSize = 0);
    static QnGraphicsMessageBox* informationTicking(const QString &text, int timeoutMsec = 0, int fontSize = 0);

public slots:
    void hideImmideately();

signals:
    void finished();
    void tick(int time);

protected:
    //virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private slots:
    void at_animationIn_finished();

private:
    GraphicsLabel *m_label;
    int m_timeout;
};

#endif // GRAPHICS_MESSAGE_BOX_H
