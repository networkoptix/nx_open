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

#include <utils/common/event_processors.h>

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

    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setVisible(true);

    ui->tableView->resizeColumnsToContents();

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzTourListModel::NameColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzTourListModel::DetailsColumn, QHeaderView::Interactive);

    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    ui->tableView->installEventFilter(this);

    ui->tableView->setItemDelegate(new QnPtzToursDialogItemDelegate(this));

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));

    ui->tableView->clearSelection();

    // TODO: #Elric replace with a single connect call
    QnSingleEventSignalizer *resizeSignalizer = new QnSingleEventSignalizer(this);
    resizeSignalizer->setEventType(QEvent::Resize);
    ui->tableView->viewport()->installEventFilter(resizeSignalizer);
    connect(resizeSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_tableViewport_resizeEvent()), Qt::QueuedConnection);


    connect(ui->tourEditWidget, SIGNAL(tourSpotsChanged(QnPtzTourSpotList)), this, SLOT(at_tourSpotsChanged(QnPtzTourSpotList)));

    connect(ui->addTourButton,      SIGNAL(clicked()), this, SLOT(at_addTourButton_clicked()));
    connect(ui->deleteTourButton,   SIGNAL(clicked()), this, SLOT(at_deleteTourButton_clicked()));
}

QnPtzToursDialog::~QnPtzToursDialog() {
}

void QnPtzToursDialog::loadData(const QnPtzData &data) {
    ui->tourEditWidget->setPresets(data.presets);
    m_model->setTours(data.tours);
    m_model->setPresets(data.presets);

    if (!data.tours.isEmpty())
        ui->tableView->setCurrentIndex(ui->tableView->model()->index(0, 0));

    ui->tableView->resizeColumnsToContents();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
}

bool QnPtzToursDialog::saveTours() {
    bool result = true;

    foreach (const QnPtzTourItemModel &model, m_model->tourModels()) {
        if (!model.modified)
            continue;

        QnPtzTour updated(model.tour);
        if (!model.local)
            result &= removeTour(updated.id);

        result &= createTour(updated);
    }

    foreach (const QString &id, m_model->removedTours())
        result &= removeTour(id);

    return result;
}

void QnPtzToursDialog::saveData() {
    saveTours();
}

Qn::PtzDataFields QnPtzToursDialog::requiredFields() const {
    return Qn::PresetsPtzField | Qn::ToursPtzField;
}

void QnPtzToursDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)
    if (!current.isValid()) {
        ui->stackedWidget->setCurrentWidget(ui->noTourPage);
        ui->deleteTourButton->setDisabled(true);
        m_currentTourId = QString();
        return;
    }


    QnPtzTour tour = current.data(Qn::PtzTourRole).value<QnPtzTour>();
    ui->tourEditWidget->setSpots(tour.spots);
    m_currentTourId = tour.id;
    ui->stackedWidget->setCurrentWidget(ui->tourPage);
    ui->deleteTourButton->setEnabled(true);
}

void QnPtzToursDialog::at_addTourButton_clicked() {
    m_model->insertRow(m_model->rowCount());
    ui->tableView->setCurrentIndex(m_model->index(m_model->rowCount() - 1, 0));
    if (m_model->rowCount() == 1) {
        ui->tableView->resizeColumnsToContents();
        ui->tableView->horizontalHeader()->setStretchLastSection(true);
        ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    }

    ui->tableView->selectionModel()->clear();
    ui->tableView->selectionModel()->setCurrentIndex(m_model->index(m_model->rowCount()-1, 0), QItemSelectionModel::Select);
    ui->tableView->selectionModel()->select(m_model->index(m_model->rowCount()-1, 0), QItemSelectionModel::Select);
}

void QnPtzToursDialog::at_deleteTourButton_clicked() {
    QModelIndex index = ui->tableView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    m_model->removeRow(index.row());
}

void QnPtzToursDialog::at_tableViewport_resizeEvent() {
    QModelIndexList selectedIndices = ui->tableView->selectionModel()->selectedRows();
    if(selectedIndices.isEmpty())
        return;

    ui->tableView->scrollTo(selectedIndices.front());
}

void QnPtzToursDialog::at_tourSpotsChanged(const QnPtzTourSpotList &spots) {
    if (m_currentTourId.isEmpty())
        return;
    m_model->updateTourSpots(m_currentTourId, spots);
}
