#ifndef GRAPHICS_MESSAGE_BOX_H
#define GRAPHICS_MESSAGE_BOX_H

#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsLinearLayout>

#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/animation/animated.h>

const int defaultMessageTimeout = 3;

class QnGraphicsMessageBoxItem: public QGraphicsWidget
{
    Q_OBJECT
    typedef QGraphicsWidget base_type;

public:
    explicit QnGraphicsMessageBoxItem(QGraphicsItem *parent = NULL);
    virtual ~QnGraphicsMessageBoxItem();
    virtual QRectF boundingRect() const override;

    void addItem(QGraphicsLayoutItem* item);
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
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = NULL);
    ~QnGraphicsMessageBox();
    
    static void information(const QString &text);
protected:
    virtual int getTimeout();
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};

#endif // GRAPHICS_MESSAGE_BOX_H
