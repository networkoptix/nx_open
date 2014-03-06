#ifndef GRAPHICS_MESSAGE_BOX_H
#define GRAPHICS_MESSAGE_BOX_H

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsLinearLayout>

#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/animation/animated.h>

// TODO: #Elric rename, not standard item => no "graphics" prefix.

const int defaultMessageTimeout = 3;

class QnGraphicsMessageBoxItem: public GraphicsWidget
{
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

class QnGraphicsMessageBox : public Animated<GraphicsLabel>
{
    Q_OBJECT

    typedef Animated<GraphicsLabel> base_type;
public:
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = NULL, const QString &text = QString(), int timeoutMsec = 0);
    ~QnGraphicsMessageBox();
    
    static QnGraphicsMessageBox* information(const QString &text);

    int timeout() const;

public slots:
    void hideImmideately();

signals:
    void finished();
    void tick(int time);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private slots:
    void at_animationIn_finished();

private:
    int m_timeout;

};

#endif // GRAPHICS_MESSAGE_BOX_H
