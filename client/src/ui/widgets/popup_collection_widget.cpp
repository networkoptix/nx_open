#include "popup_collection_widget.h"
#include "ui_popup_collection_widget.h"

#include <ui/widgets/popup_widget.h>

QnPopupCollectionWidget::QnPopupCollectionWidget(QWidget *parent) :
    QWidget(parent, Qt::Popup),
    ui(new Ui::QnPopupCollectionWidget)
{
    ui->setupUi(this);

    m_adding = true; //debug variable
}

QnPopupCollectionWidget::~QnPopupCollectionWidget()
{
    delete ui;
}

void QnPopupCollectionWidget::add() {

    if (m_adding) {
        QnPopupWidget* w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        w = new QnPopupWidget(this);
        ui->verticalLayout->addWidget(w);
        m_adding = false;
    } else {
        ui->verticalLayout->removeItem(ui->verticalLayout->itemAt(0));
        m_adding = ui->verticalLayout->count() == 0;
    }

}

void QnPopupCollectionWidget::showEvent(QShowEvent *event) {
    QRect pgeom = static_cast<QWidget *>(parent())->geometry();
    QRect geom = geometry();
    setGeometry(pgeom.left() + pgeom.width() - geom.width(), pgeom.top() + pgeom.height() - geom.height(), geom.width(), geom.height());
    base_type::showEvent(event);
}
