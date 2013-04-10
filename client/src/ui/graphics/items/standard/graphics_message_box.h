#ifndef GRAPHICS_MESSAGE_BOX_H
#define GRAPHICS_MESSAGE_BOX_H

#include <QtGui/QGraphicsView>

#include <ui/graphics/items/standard/graphics_label.h>

const int defaultMessageTimeout = 3;

class QnGraphicsMessageBox : public GraphicsLabel
{
    Q_OBJECT

    typedef GraphicsLabel base_type;
public:
    explicit QnGraphicsMessageBox(QGraphicsItem *parent = NULL);
    ~QnGraphicsMessageBox();
    
    static void information(QGraphicsItem* item, const QString &text);
protected:
    virtual int getTimeout();
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
};

#endif // GRAPHICS_MESSAGE_BOX_H
