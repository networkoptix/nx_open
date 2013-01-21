#include "popup_widget.h"
#include "ui_popup_widget.h"

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget),
    m_actionType(BusinessActionType::BA_NotDefined)
{
    ui->setupUi(this);
    int r = qrand();
    if (r % 2)
        ui->label_2->setVisible(false);
    else
        ui->label_3->setVisible(false);

    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(at_okButton_clicked()));
}

QnPopupWidget::~QnPopupWidget()
{
    delete ui;
}

void QnPopupWidget::at_okButton_clicked() {
    emit closed(m_actionType);
}
