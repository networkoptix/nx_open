#include "ptz_manage_dialog.h"
#include "ui_ptz_manage_dialog.h"

#include <QtCore/QUuid>
#include <QtCore/QScopedPointer>

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QMessageBox>

#include <common/common_globals.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_tour.h>

#include <ui/delegates/ptz_preset_hotkey_item_delegate.h>
#include <ui/common/ui_resource_name.h>
#include <ui/models/ptz_manage_model.h>
#include <ui/widgets/ptz_tour_widget.h>

#include <utils/common/event_processors.h>
#include <utils/resource_property_adaptors.h>

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

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (index.column() == QnPtzManageModel::HotkeyColumn)
            return hotkeyDelegate.createEditor(parent, option, index);
        return base_type::createEditor(parent, option, index);
    }

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        if (index.column() == QnPtzManageModel::HotkeyColumn)
            hotkeyDelegate.setEditorData(editor, index);
        else
            base_type::setEditorData(editor, index);
    }

    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        if (index.column() == QnPtzManageModel::HotkeyColumn)
            hotkeyDelegate.setModelData(editor, model, index);
        else
            base_type::setModelData(editor, model, index);
    }

private:
    QnPtzPresetHotkeyItemDelegate hotkeyDelegate;
};

//TODO:  allow to maximize dialog
QnPtzManageDialog::QnPtzManageDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::PtzManageDialog),
    m_model(new QnPtzManageModel(this))
{
    ui->setupUi(this);

    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setVisible(true);

    ui->tableView->resizeColumnsToContents();
    
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzManageModel::NameColumn, QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzManageModel::DetailsColumn, QHeaderView::Interactive);

    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    ui->tableView->installEventFilter(this);

    ui->tableView->setItemDelegate(new QnPtzToursDialogItemDelegate(this));

    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(at_tableView_currentRowChanged(QModelIndex,QModelIndex)));
    connect(ui->tableView->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(updateUi()));

    ui->tableView->clearSelection();

    // TODO: #Elric replace with a single connect call
    QnSingleEventSignalizer *resizeSignalizer = new QnSingleEventSignalizer(this);
    resizeSignalizer->setEventType(QEvent::Resize);
    ui->tableView->viewport()->installEventFilter(resizeSignalizer);
    connect(resizeSignalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(at_tableViewport_resizeEvent()), Qt::QueuedConnection);

    connect(m_model,    &QnPtzManageModel::presetsChanged,  ui->tourEditWidget, &QnPtzTourWidget::setPresets);
    connect(this,       &QnAbstractPtzDialog::synchronized, m_model,            &QnPtzManageModel::setSynchronized);
    connect(ui->tourEditWidget, SIGNAL(tourSpotsChanged(QnPtzTourSpotList)), this, SLOT(at_tourSpotsChanged(QnPtzTourSpotList)));

    //connect(ui->savePositionButton, &QPushButton::clicked,  this,   &QnPtzManageDialog::at_savePositionButton_clicked);
    //connect(ui->goToPositionButton, &QPushButton::clicked,  this,   &QnPtzManageDialog::at_goToPositionButton_clicked);
    connect(ui->addTourButton,      &QPushButton::clicked,  this,   &QnPtzManageDialog::at_addTourButton_clicked);
    //connect(ui->startTourButton,    &QPushButton::clicked,  this,   &QnPtzManageDialog::at_startTourButton_clicked);
    //connect(ui->deleteButton,       &QPushButton::clicked,  this,   &QnPtzManageDialog::at_deleteButton_clicked);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,   this, &QnAbstractPtzDialog::saveChanges);
    //TODO: enable and disable various gui elements:
    /*
        - Apply - only if there are some changes
        - GoToPosition - if there is a selected position or a tour spot
        - CreateTour - if there is at least one position
        - ActivateTour - if there is a selected tour
        - Apply
    */

    //TODO: implement preview receiving and displaying

    //TODO: handle HomePosition
    //TODO: handle resource switching ("Save changes? Yes/No/Cancel")
    //TODO: Show warning if Home Position is set (ask Borya about text)
    //TODO: handle new positions added from the context menu (controller->changed() signal, Elric will implement controller side himself)
    //TODO: think about forced refresh in some cases or even a button - low priority
}

QnPtzManageDialog::~QnPtzManageDialog() {
}

void QnPtzManageDialog::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        event->ignore();
        return;
    default:
        break;
    }
    base_type::keyPressEvent(event);
}


void QnPtzManageDialog::loadData(const QnPtzData &data) {
    // ui->tableView->setColumnHidden(QnPtzTourListModel::HomeColumn, !(capabilities() & Qn::HomePtzCapability)); //TODO: uncomment
    m_model->setTours(data.tours);
    m_model->setPresets(data.presets);
    m_model->setHomePosition(data.homeObject.id);

    if (m_resource) {
        QnPtzHotkeysResourcePropertyAdaptor adaptor(m_resource);
        m_model->setHotkeys(adaptor.value());
    }

    if (!data.tours.isEmpty())
        ui->tableView->setCurrentIndex(ui->tableView->model()->index(0, 0));

    ui->tableView->resizeColumnsToContents();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
}

bool QnPtzManageDialog::savePresets() {
    bool result = true;

    foreach (const QnPtzPresetItemModel &model, m_model->presetModels()) {
        if (!model.modified)
            continue;

        if (model.local)
            result &= createPreset(model.preset);
        else
            result &= updatePreset(model.preset);
    }

    foreach (const QString &id, m_model->removedPresets())
        result &= removePreset(id);

    return result;
}

bool QnPtzManageDialog::saveTours() {
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

void QnPtzManageDialog::saveData() {
    if (!m_model->synchronized()) {
        savePresets();
        saveTours();
    }

    if (m_resource) {
        QnPtzHotkeysResourcePropertyAdaptor adaptor(m_resource);
        adaptor.setValue(m_model->hotkeys());
    }
}

Qn::PtzDataFields QnPtzManageDialog::requiredFields() const {
    return Qn::PresetsPtzField | Qn::ToursPtzField | Qn::HomeObjectPtzField;
}

void QnPtzManageDialog::at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous)

    QString currentPresetId;
    m_currentTourId = QString();
    if (current.isValid()) {
        QnPtzManageModel::RowData data = m_model->rowData(current.row());

        switch (data.rowType) {
        case QnPtzManageModel::PresetRow:
            currentPresetId = data.presetModel.preset.id;
            break;
        case QnPtzManageModel::TourRow:
            ui->tourEditWidget->setSpots(data.tourModel.tour.spots);
            m_currentTourId = data.tourModel.tour.id;
            break;
        default:
            break;
        }
    }

    //ui->deleteButton->setDisabled(currentPresetId.isEmpty() && m_currentTourId.isEmpty());
    //ui->tourStackedWidget->setCurrentWidget(m_currentTourId.isEmpty()
        //? ui->noTourPage
        //: ui->tourPage);
}

void QnPtzManageDialog::at_savePositionButton_clicked() {
    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == QnResource::Offline || m_resource->getStatus() == QnResource::Unauthorized) {
        QMessageBox::critical(
            this,
            tr("Could not get position from camera"),
            tr("An error has occurred while trying to get current position from camera %1.\n\n"\
            "Please wait for the camera to go online.").arg(m_resource->getName())
            );
        return;
    }

    m_model->addPreset();
    saveChanges();
}

void QnPtzManageDialog::at_goToPositionButton_clicked() {
    QModelIndex index = ui->tableView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == QnResource::Offline || m_resource->getStatus() == QnResource::Unauthorized) {
        QMessageBox::critical(
            this,
            tr("Could not set position for camera"),
            tr("An error has occurred while trying to set current position for camera %1.\n\n"\
            "Please wait for the camera to go online.").arg(m_resource->getName())
            );
        return;
    }

    QnPtzManageModel::RowData data = m_model->rowData(index.row());
    switch (data.rowType) {
    case QnPtzManageModel::PresetRow:
        controller()->activatePreset(data.presetModel.preset.id, 1.0);
        break;
    case QnPtzManageModel::TourRow:
        //TODO: get id of the selected spot
        break;
    default:
        break;
    }
}

void QnPtzManageDialog::at_startTourButton_clicked() {
    QModelIndex index = ui->tableView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == QnResource::Offline || m_resource->getStatus() == QnResource::Unauthorized) {
        QMessageBox::critical(
            this,
            tr("Could not set position for camera"),
            tr("An error has occurred while trying to set current position for camera %1.\n\n"\
            "Please wait for the camera to go online.").arg(m_resource->getName())
            );
        return;
    }

    QnPtzManageModel::RowData data = m_model->rowData(index.row());
    switch (data.rowType) {
    case QnPtzManageModel::TourRow:
        controller()->activateTour(data.tourModel.tour.id);
        break;
    default:
        break;
    }
}

void QnPtzManageDialog::at_addTourButton_clicked() {
    //bool wasEmpty = m_model->rowCount() == 0;
    m_model->addTour();
    QModelIndex index = m_model->index(m_model->rowCount() - 1, 0);
    ui->tableView->setCurrentIndex(index);
    //if (wasEmpty) { //TODO: check if needed
        ui->tableView->resizeColumnsToContents();
        ui->tableView->horizontalHeader()->setStretchLastSection(true);
        ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
    //}

    ui->tableView->selectionModel()->clear();
    ui->tableView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
    ui->tableView->selectionModel()->select(index, QItemSelectionModel::Select);
}

void QnPtzManageDialog::at_deleteButton_clicked() {
    QModelIndex index = ui->tableView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    QnPtzManageModel::RowData data = m_model->rowData(index.row());
    switch (data.rowType) {
    case QnPtzManageModel::PresetRow:
        //TODO: check if this preset is used in some tours and show a warning message (Ok/Cancel, optionally: "Do not show anymore", session-only)
        m_model->removePreset(data.presetModel.preset.id);
        break;
    case QnPtzManageModel::TourRow:
        m_model->removeTour(data.tourModel.tour.id);
        break;
    default:
        break;
    }
}

void QnPtzManageDialog::at_tableViewport_resizeEvent() {
    QModelIndexList selectedIndices = ui->tableView->selectionModel()->selectedRows();
    if(selectedIndices.isEmpty())
        return;

    ui->tableView->scrollTo(selectedIndices.front());
}

void QnPtzManageDialog::at_tourSpotsChanged(const QnPtzTourSpotList &spots) {
    if (m_currentTourId.isEmpty())
        return;
    m_model->updateTourSpots(m_currentTourId, spots);
}

QnResourcePtr QnPtzManageDialog::resource() const {
    return m_resource;
}

void QnPtzManageDialog::setResource(const QnResourcePtr &resource) {
    if (m_resource == resource)
        return;
    m_resource = resource;
    setWindowTitle(tr("Manage PTZ for %1").arg(getResourceName(m_resource)));
}

//TODO: call and connect in required places
void QnPtzManageDialog::updateUi() {
    ui->addTourButton->setEnabled(!m_model->presetModels().isEmpty());

    QnPtzManageModel::RowType selectedRow = QnPtzManageModel::InvalidRow;
    QModelIndex index = ui->tableView->selectionModel()->currentIndex();
    if (index.isValid())
        selectedRow = m_model->rowData(index.row()).rowType;

    //ui->deleteButton->setEnabled(selectedRow == QnPtzManageModel::PresetRow || selectedRow == QnPtzManageModel::TourRow);
    //TODO: add other buttons;
}
