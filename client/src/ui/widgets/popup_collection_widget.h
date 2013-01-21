#ifndef POPUP_COLLECTION_WIDGET_H
#define POPUP_COLLECTION_WIDGET_H

#include <QWidget>

namespace Ui {
    class QnPopupCollectionWidget;
}

class QnPopupCollectionWidget : public QWidget
{
    Q_OBJECT
    typedef QWidget base_type;
    
public:
    explicit QnPopupCollectionWidget(QWidget *parent);
    ~QnPopupCollectionWidget();
    
    void add();

protected:
    virtual void showEvent(QShowEvent *event) override;

private:
    Ui::QnPopupCollectionWidget *ui;

    bool m_adding;
};

#endif // POPUP_COLLECTION_WIDGET_H
