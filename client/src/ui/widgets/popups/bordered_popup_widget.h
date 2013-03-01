#ifndef BORDERED_POPUP_WIDGET_H
#define BORDERED_POPUP_WIDGET_H

#include <QWidget>

class QnBorderedPopupWidget : public QWidget
{
    Q_OBJECT

    typedef QWidget base_type;
public:
    explicit QnBorderedPopupWidget(QWidget *parent = 0);
    virtual ~QnBorderedPopupWidget();
protected:
    virtual void paintEvent(QPaintEvent *event) override;

    void setBorderColor(QColor color);
    QColor borderColor() const;

private:
    QColor m_borderColor;
};

#endif // BORDERED_POPUP_WIDGET_H
