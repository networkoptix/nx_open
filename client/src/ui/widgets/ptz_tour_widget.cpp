#include "ptz_tour_widget.h"
#include "ui_ptz_tour_widget.h"

#include <core/ptz/ptz_tour.h>

#include <ui/models/ptz_tour_model.h>

QnPtzTourWidget::QnPtzTourWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPtzTourWidget),
    m_model(new QnPtzTourModel(this))
{
    ui->setupUi(this);
    ui->treeView->setModel(m_model);
}

QnPtzTourWidget::~QnPtzTourWidget() {
}


void QnPtzTourWidget::setPtzTour(const QnPtzTour &tour) {
    m_model->setTour(tour);
    ui->nameLineEdit->setText(tour.name);
}
