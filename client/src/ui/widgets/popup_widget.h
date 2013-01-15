#ifndef POPUP_WIDGET_H
#define POPUP_WIDGET_H

#include <QWidget>

namespace Ui {
    class QnPopupWidget;
}

class QnPopupWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit QnPopupWidget(QWidget *parent = 0);
    ~QnPopupWidget();
    
private:
    Ui::QnPopupWidget *ui;
};

#endif // POPUP_WIDGET_H
