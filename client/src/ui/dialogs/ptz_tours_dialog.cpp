#include "ptz_tours_dialog.h"
#include "ui_ptz_tours_dialog.h"

#include <QtCore/QUuid>

#include <common/common_globals.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_tour.h>

#include <ui/models/ptz_tour_list_model.h>

#include <ui/widgets/ptz_tour_widget.h>

QnPtzToursDialog::QnPtzToursDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::QnPtzToursDialog),
    m_model(new QnPtzTourListModel(this))
{
    ui->setupUi(this);
    ui->tourTable->setModel(m_model);

    connect(ui->tourTable->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(at_table_currentRowChanged(QModelIndex,QModelIndex)));
    connect(ui->tourEditWidget, SIGNAL(tourChanged(QnPtzTour)), m_model, SLOT(updateTour(QnPtzTour)));

    connect(ui->addTourButton,      SIGNAL(clicked()), this, SLOT(at_addTourButton_clicked()));
    connect(ui->deleteTourButton,   SIGNAL(clicked()), this, SLOT(at_deleteTourButton_clicked()));
}

QnPtzToursDialog::~QnPtzToursDialog() {
}

const QnPtzControllerPtr& QnPtzToursDialog::ptzController() const {
    return m_controller;
}

void QnPtzToursDialog::setPtzController(const QnPtzControllerPtr &controller) {
    if(m_controller == controller)
        return;

    m_controller = controller;
    updateModel();
}


void QnPtzToursDialog::accept() {
    if (m_controller) {
        foreach (const QnPtzTour &tour, m_model->tours()) {
            qDebug() <<"updating tour" << tour.name << tour.id;
            m_controller->createTour(tour);
        }
        //TODO: #GDM PTZ remove deleted tours
    }

    base_type::accept();
}

void QnPtzToursDialog::updateModel() {
    QnPtzTourList tours;
    QnPtzPresetList presets;
    if (m_controller && m_controller->getTours(&tours) && m_controller->getPresets(&presets)) {
        ui->tourEditWidget->setPtzPresets(presets);
        m_model->setTours(tours);
    }
    if (!tours.isEmpty())
        ui->tourTable->setCurrentIndex(ui->tourTable->model()->index(0, 0));
}

void QnPtzToursDialog::at_table_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)
    if (!current.isValid()) {
        ui->stackedWidget->setCurrentWidget(ui->noTourPage);
        ui->deleteTourButton->setDisabled(true);
        return;
    }

    ui->tourEditWidget->setPtzTour(current.data(Qn::PtzTourRole).value<QnPtzTour>());
    ui->stackedWidget->setCurrentWidget(ui->tourPage);
    ui->deleteTourButton->setEnabled(true);
}

void QnPtzToursDialog::at_addTourButton_clicked() {
    m_model->insertRow(m_model->rowCount());
    ui->tourTable->selectionModel()->clear();
    ui->tourTable->selectionModel()->setCurrentIndex(m_model->index(m_model->rowCount()-1, 0), QItemSelectionModel::Select);
    ui->tourTable->selectionModel()->select(m_model->index(m_model->rowCount()-1, 0), QItemSelectionModel::Select);
}

void QnPtzToursDialog::at_deleteTourButton_clicked() {
    QModelIndex index = ui->tourTable->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    m_model->removeRow(index.row());
}
