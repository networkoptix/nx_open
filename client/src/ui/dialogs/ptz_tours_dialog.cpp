#include "ptz_tours_dialog.h"
#include "ui_ptz_tours_dialog.h"

#include <QtCore/QUuid>

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QMessageBox>

#include <common/common_globals.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_tour.h>

#include <ui/models/ptz_tour_list_model.h>

#include <ui/widgets/ptz_tour_widget.h>

class QnPtzToursDialogItemDelegate: public QStyledItemDelegate {
    typedef QStyledItemDelegate base_type;
public:
    explicit QnPtzToursDialogItemDelegate(QObject *parent = 0): base_type(parent) {}
    ~QnPtzToursDialogItemDelegate() {}
protected:
    virtual void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        base_type::initStyleOption(option, index);
        if (!index.data(Qn::ValidRole).toBool()) {
            QColor clr = index.data(Qt::BackgroundRole).value<QColor>();
            option->palette.setColor(QPalette::Highlight, clr.lighter());
        }
    }
};

QnPtzToursDialog::QnPtzToursDialog(const QnPtzControllerPtr &controller, QWidget *parent) :
    base_type(controller, parent),
    ui(new Ui::PtzToursDialog),
    m_model(new QnPtzTourListModel(this))
{
    ui->setupUi(this);
    ui->tourTable->setModel(m_model);

    connect(ui->tourTable->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(at_table_currentRowChanged(QModelIndex,QModelIndex)));
    ui->tourTable->setItemDelegate(new QnPtzToursDialogItemDelegate(this));

    connect(ui->tourEditWidget, SIGNAL(tourChanged(QnPtzTour)), m_model, SLOT(updateTour(QnPtzTour)));

    connect(ui->addTourButton,      SIGNAL(clicked()), this, SLOT(at_addTourButton_clicked()));
    connect(ui->deleteTourButton,   SIGNAL(clicked()), this, SLOT(at_deleteTourButton_clicked()));
}

QnPtzToursDialog::~QnPtzToursDialog() {
}

void QnPtzToursDialog::loadData(const QnPtzData &data) {
    m_oldTours = data.tours;

    ui->tourEditWidget->setPtzPresets(data.presets);
    m_model->setTours(data.tours);
    m_model->setPresets(data.presets);

    if (!data.tours.isEmpty())
        ui->tourTable->setCurrentIndex(ui->tourTable->model()->index(0, 0));
}

bool QnPtzToursDialog::saveTours() {
    auto findTourById = [](const QnPtzTourList &list, const QString &id, QnPtzTour &result) {
        foreach (const QnPtzTour &tour, list) {
            if (tour.id != id)
                continue;
            result = tour;
            return true;
        }
        return false;
    };

    bool result = true;
    // update or remove existing tours
    foreach (const QnPtzTour &tour, m_oldTours) {
        QnPtzTour updated;
        if (!findTourById(m_model->tours(), tour.id, updated))
            result &= removeTour(tour.id);
        else if (tour != updated)
            result &= createTour(updated);
    }

    //create new tours
    foreach (const QnPtzTour &tour, m_model->tours()) {
        QnPtzTour existing;
        if (findTourById(m_oldTours, tour.id, existing))
            continue;
        result &= createTour(tour);
    }

    return result;
}

void QnPtzToursDialog::saveData() {
    saveTours();
}

Qn::PtzDataFields QnPtzToursDialog::requiredFields() const {
    return Qn::PresetsPtzField | Qn::ToursPtzField;
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
