#ifndef QN_CONTROL_BACKGROUND_ITEM_H
#define QN_CONTROL_BACKGROUND_ITEM_H

#include <common/common_globals.h>

#include <ui/graphics/items/generic/framed_widget.h>

class QnControlBackgroundWidget: public QnFramedWidget {
    Q_OBJECT
    Q_PROPERTY(QVector<QColor> colors READ colors WRITE setColors)
    typedef QnFramedWidget base_type;

public:
    QnControlBackgroundWidget(QGraphicsItem *parent = NULL);
    QnControlBackgroundWidget(Qn::Border gradientBorder, QGraphicsItem *parent = NULL);

    Qn::Border gradientBorder() const;
    void setGradientBorder(Qn::Border gradientBorder);

    const QVector<QColor> &colors() const;
    void setColors(const QVector<QColor> &colors);

private:
    void init(Qn::Border gradientBorder);

    void updateWindowBrush();

private:
    Qn::Border m_gradientBorder;
    QVector<QColor> m_colors;
};

#endif // QN_CONTROL_BACKGROUND_ITEM_H
