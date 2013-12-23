#include "ptz_tour_widget.h"
#include "ui_ptz_tour_widget.h"

#include <core/ptz/ptz_tour.h>

#include <ui/models/ptz_tour_model.h>
#include <ui/delegates/ptz_tour_item_delegate.h>

QnPtzTourWidget::QnPtzTourWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPtzTourWidget),
    m_model(new QnPtzTourModel(this))
{
    ui->setupUi(this);
    ui->treeView->setModel(m_model);

    connect(m_model,            SIGNAL(tourChanged(QnPtzTour)), this, SIGNAL(tourChanged(QnPtzTour)));
    connect(ui->nameLineEdit,   SIGNAL(textChanged(QString)),   m_model, SLOT(setTourName(QString)));

    connect(ui->addSpotButton,      SIGNAL(clicked()), this, SLOT(at_addSpotButton_clicked()));
    connect(ui->deleteSpotButton,   SIGNAL(clicked()), this, SLOT(at_deleteSpotButton_clicked()));
    connect(ui->moveSpotUpButton,   SIGNAL(clicked()), this, SLOT(at_moveSpotUpButton_clicked()));
    connect(ui->moveSpotDownButton, SIGNAL(clicked()), this, SLOT(at_moveSpotDownButton_clicked()));

    ui->treeView->setItemDelegate(new QnPtzTourItemDelegate(this));
}

QnPtzTourWidget::~QnPtzTourWidget() {
}


void QnPtzTourWidget::setPtzTour(const QnPtzTour &tour) {
    m_model->setTour(tour);
    ui->nameLineEdit->setText(tour.name);
}

void QnPtzTourWidget::setPtzPresets(const QnPtzPresetList &presets) {
    m_model->setPresets(presets);
}

void QnPtzTourWidget::at_addSpotButton_clicked() {
    m_model->insertRow(m_model->rowCount());
}

void QnPtzTourWidget::at_deleteSpotButton_clicked() {
    QModelIndex index = ui->treeView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    m_model->removeRow(index.row());
}

void QnPtzTourWidget::at_moveSpotUpButton_clicked() {
    QModelIndex index = ui->treeView->selectionModel()->currentIndex();
    if (!index.isValid() || index.row() == 0)
        return;

    m_model->moveRow(QModelIndex(), index.row(), QModelIndex(), index.row() - 1);
}

void QnPtzTourWidget::at_moveSpotDownButton_clicked() {
    QModelIndex index = ui->treeView->selectionModel()->currentIndex();
    if (!index.isValid() || index.row() == m_model->rowCount() - 1)
        return;

    m_model->moveRow(QModelIndex(), index.row(), QModelIndex(), index.row() + 1);
}
