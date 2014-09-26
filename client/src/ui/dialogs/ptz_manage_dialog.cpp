#include "ptz_manage_dialog.h"
#include "ui_ptz_manage_dialog.h"

#include <utils/common/uuid.h>
#include <QtCore/QScopedPointer>

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QMessageBox>

#include <common/common_globals.h>
#include <client/client_settings.h>

#include <utils/common/event_processors.h>
#include <utils/common/string.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/resource_property_adaptors.h>
#include <utils/local_file_cache.h>
#include <utils/threaded_image_loader.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_tour.h>

#include <ui/delegates/ptz_preset_hotkey_item_delegate.h>
#include <ui/common/ui_resource_name.h>
#include <ui/widgets/ptz_tour_widget.h>
#include <ui/dialogs/checkable_message_box.h>
#include <ui/dialogs/message_box.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

class QnPtzToursDialogItemDelegate: public QStyledItemDelegate {
    typedef QStyledItemDelegate base_type;
public:
    explicit QnPtzToursDialogItemDelegate(QObject *parent = 0): base_type(parent) {}
    ~QnPtzToursDialogItemDelegate() {}

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

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize result = base_type::sizeHint(option, index);

        if(index.column() == QnPtzManageModel::HotkeyColumn)
            result += QSize(48, 0); /* Some sane expansion to accommodate combo box contents. */

        return result;
    }

private:
    QnPtzPresetHotkeyItemDelegate hotkeyDelegate;
};

QnPtzManageDialog::QnPtzManageDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::PtzManageDialog),
    m_model(new QnPtzManageModel(this)),
    m_adaptor(new QnPtzHotkeysResourcePropertyAdaptor(this)),
    m_cache(new QnLocalFileCache(this)),
    m_submitting(false)
{
    ui->setupUi(this);

    ui->previewGroupBox->hide(); // TODO: #dklychkov implement preview fetching and remove this line

    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setVisible(true);

    ui->tableView->resizeColumnsToContents();
    
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzManageModel::DetailsColumn, QHeaderView::Stretch);

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
    connect(m_model,    &QnPtzManageModel::presetsChanged,  this,               &QnPtzManageDialog::updateUi);
    connect(m_model,    &QnPtzManageModel::dataChanged,     this,               &QnPtzManageDialog::updateUi);
    connect(m_model,    &QnPtzManageModel::modelReset,      this,               &QnPtzManageDialog::updateUi);
    connect(m_model,    &QnPtzManageModel::modelAboutToBeReset, this,           &QnPtzManageDialog::at_model_modelAboutToBeReset);
    connect(m_model,    &QnPtzManageModel::modelReset,      this,               &QnPtzManageDialog::at_model_modelReset);
    connect(this,       &QnAbstractPtzDialog::synchronized, m_model,            &QnPtzManageModel::setSynchronized);
    connect(ui->tourEditWidget, SIGNAL(tourSpotsChanged(QnPtzTourSpotList)), this, SLOT(at_tourSpotsChanged(QnPtzTourSpotList)));
    connect(m_cache,    &QnLocalFileCache::fileDownloaded,  this,               &QnPtzManageDialog::at_cache_imageLoaded);
    connect(m_adaptor,  &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnPtzManageDialog::updateHotkeys);

    connect(ui->savePositionButton, &QPushButton::clicked,  this,   &QnPtzManageDialog::at_savePositionButton_clicked);
    connect(ui->goToPositionButton, &QPushButton::clicked,  this,   &QnPtzManageDialog::at_goToPositionButton_clicked);
    connect(ui->addTourButton,      &QPushButton::clicked,  this,   &QnPtzManageDialog::at_addTourButton_clicked);
    connect(ui->startTourButton,    &QPushButton::clicked,  this,   &QnPtzManageDialog::at_startTourButton_clicked);
    connect(ui->deleteButton,       &QPushButton::clicked,  this,   &QnPtzManageDialog::at_deleteButton_clicked);
    connect(ui->getPreviewButton,   &QPushButton::clicked,  this,   &QnPtzManageDialog::at_getPreviewButton_clicked);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,   this, &QnAbstractPtzDialog::saveChanges);

    setHelpTopic(ui->tourGroupBox, Qn::PtzManagement_Tour_Help);

    //TODO: implement preview receiving and displaying

    //TODO: think about forced refresh in some cases or even a button - low priority
}

QnPtzManageDialog::~QnPtzManageDialog() {
    return;
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

void QnPtzManageDialog::reject() {
    if (!tryClose(false))
        return;

    clear();
    base_type::reject();
}

void QnPtzManageDialog::accept() {
    saveData();

    clear();
    QDialog::accept(); // here we skip QnAbstractPtzDialog::accept because we don't want call synchronize()
}

void QnPtzManageDialog::loadData(const QnPtzData &data) {
    QnPtzPresetList presets = data.presets;
    QnPtzTourList tours = data.tours;
    qSort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });
    qSort(tours.begin(), tours.end(), [](const QnPtzTour &l, const QnPtzTour &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });

    m_model->setTours(tours);
    m_model->setPresets(presets);
    m_model->setHomePosition(data.homeObject.id);

    if (!data.tours.isEmpty())
        ui->tableView->setCurrentIndex(ui->tableView->model()->index(0, 0));

    updateHotkeys();

    ui->tableView->resizeColumnsToContents();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->horizontalHeader()->setCascadingSectionResizes(true);
}

bool QnPtzManageDialog::savePresets() {
    bool result = true;

    QStringList removedPresets = m_model->removedPresets();

    foreach (const QnPtzPresetItemModel &model, m_model->presetModels()) {
        if (!model.modified)
            continue;

        if (model.local)
            result &= createPreset(model.preset);
        else
            result &= updatePreset(model.preset);
    }

    foreach (const QString &id, removedPresets)
        result &= removePreset(id);

    return result;
}

bool QnPtzManageDialog::saveTours() {
    bool result = true;

    QStringList removedTours = m_model->removedTours();

    foreach (const QnPtzTourItemModel &model, m_model->tourModels()) {
        if (!model.modified)
            continue;

        /* There is no need to remove the tour first as it will be updated
         * in-place if it already exists. */
        result &= createTour(model.tour);
    }

    foreach (const QString &id, removedTours)
        result &= removeTour(id);

    return result;
}

bool QnPtzManageDialog::saveHomePosition() {
    if (!m_model->isHomePositionChanged())
        return false;

    QnPtzManageModel::RowData rowData = m_model->rowData(m_model->homePosition());
    QnPtzObject homePosition;
    switch (rowData.rowType) {
    case QnPtzManageModel::PresetRow:
        homePosition = QnPtzObject(Qn::PresetPtzObject, rowData.id());
        break;
    case QnPtzManageModel::TourRow:
        homePosition = QnPtzObject(Qn::TourPtzObject, rowData.id());
        break;
    default:
        break; /* Still need to update it. */
    }

    return updateHomePosition(homePosition);
}

void QnPtzManageDialog::enableDewarping() {
    QList<QnResourceWidget *> widgets = display()->widgets(m_resource);
    if (widgets.isEmpty())
        return;

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets.first());
    if (!widget)
        return;

    if (widget->dewarpingParams().enabled) {
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }
}

void QnPtzManageDialog::clear() {
    // ensure that no old data will stay in dialog
    setController(QnPtzControllerPtr());
    m_model->setPresets(QnPtzPresetList());
    m_model->setTours(QnPtzTourList());
    ui->tourEditWidget->setSpots(QnPtzTourSpotList());
    ui->tourEditWidget->setPresets(QnPtzPresetList());
    ui->tourStackedWidget->setCurrentIndex(ui->tourStackedWidget->indexOf(ui->noTourPage));
}

void QnPtzManageDialog::saveData() {
    QN_SCOPED_VALUE_ROLLBACK(&m_submitting, true);

    if (!m_model->synchronized()) {
        savePresets();
        saveTours();
        saveHomePosition();
    }

    m_adaptor->setValue(m_model->hotkeys());

    // reset active ptz object if the current active ptz object is an empty tour
    QnPtzObject ptzObject;
    if (controller()->getActiveObject(&ptzObject) && ptzObject.type == Qn::TourPtzObject) {
        QnPtzManageModel::RowData rowData = m_model->rowData(ptzObject.id);
        if (!rowData.tourModel.tour.isValid(m_model->presets()))
            controller()->continuousMove(QVector3D(0, 0, 0)); // #TODO: #dklychkov evil hack to reset active object. We should implement an adequate way to do this
    }
}

Qn::PtzDataFields QnPtzManageDialog::requiredFields() const {
    return Qn::PresetsPtzField | Qn::ToursPtzField | Qn::HomeObjectPtzField;
}

void QnPtzManageDialog::updateFields(Qn::PtzDataFields fields) {
    // TODO: #dklychkov make incremental changes instead of resetting the model (low priority)

    if(m_submitting)
        return;

    if (fields & Qn::PresetsPtzField) {
        QnPtzPresetList presets;
        if (controller()->getPresets(&presets)) {
            qSort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
                return naturalStringCaseInsensitiveLessThan(l.name, r.name);
            });
            m_model->setPresets(presets);
        }
    }

    if (fields & Qn::ToursPtzField) {
        QnPtzTourList tours;
        if (controller()->getTours(&tours)) {
            qSort(tours.begin(), tours.end(), [](const QnPtzTour &l, const QnPtzTour &r) {
                return naturalStringCaseInsensitiveLessThan(l.name, r.name);
            });
            m_model->setTours(tours);
        }
    }

    if (fields & Qn::HomeObjectPtzField) {
        QnPtzObject homeObject;
        if (controller()->getHomeObject(&homeObject))
            m_model->setHomePosition(homeObject.id);
    }

    if (fields & Qn::ActiveObjectPtzField) {
        QnPtzObject ptzObject;
        if (controller()->getActiveObject(&ptzObject) && m_pendingPreviews.contains(ptzObject.id)) {
            m_pendingPreviews.remove(ptzObject.id);
            storePreview(ptzObject.id);
        }
    }
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

    ui->tourStackedWidget->setCurrentWidget(m_currentTourId.isEmpty() ? ui->noTourPage : ui->tourPage);
}

void QnPtzManageDialog::at_savePositionButton_clicked() {
    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == Qn::Offline || m_resource->getStatus() == Qn::Unauthorized) {
        QMessageBox::critical(
            this,
            tr("Could not get position from camera"),
            tr("An error has occurred while trying to get current position from camera %1.\n\n"
            "Please wait for the camera to go online.").arg(m_resource->getName())
            );
        return;
    }

    m_model->addPreset();
    saveChanges(); // TODO: #dklychkov remove it from here and implement presets creation in some other way
}

void QnPtzManageDialog::at_goToPositionButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid())
        return;

    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == Qn::Offline || m_resource->getStatus() == Qn::Unauthorized) {
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
        enableDewarping();
        controller()->activatePreset(data.presetModel.preset.id, 1.0);
        break;
    case QnPtzManageModel::TourRow: {
        if (data.tourModel.tour.spots.isEmpty())
            break;

        QnPtzTourSpot spot = ui->tourEditWidget->currentTourSpot();
        if (!spot.presetId.isEmpty()) {
            enableDewarping();
            controller()->activatePreset(spot.presetId, 1.0);
        }

        break;
    }
    default:
        break;
    }
}

void QnPtzManageDialog::at_startTourButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid())
        return;

    if (!m_resource || !controller())
        return;

    if(m_resource->getStatus() == Qn::Offline || m_resource->getStatus() == Qn::Unauthorized) {
        QMessageBox::critical(
            this,
            tr("Could not set position for camera"),
            tr("An error has occurred while trying to set current position for camera %1.\n\n"\
            "Please wait for the camera to go online.").arg(m_resource->getName())
            );
        return;
    }

    QnPtzManageModel::RowData data = m_model->rowData(index.row());
    if (data.rowType == QnPtzManageModel::TourRow) {
        enableDewarping();
        controller()->activateTour(data.tourModel.tour.id);
    }
}

void QnPtzManageDialog::at_addTourButton_clicked() {
    //bool wasEmpty = m_model->rowCount() == 0;
    m_model->addTour();
    QModelIndex index = m_model->index(m_model->rowCount() - 1, 0);
    //if (wasEmpty) { //TODO: check if needed
        // TODO: #Elric duplicate code with QnPtzTourWidget
        for(int i = 0; i < ui->tableView->horizontalHeader()->count(); i++)
            if(ui->tableView->horizontalHeader()->sectionResizeMode(i) == QHeaderView::ResizeToContents)
                ui->tableView->resizeColumnToContents(i);
    //}

    ui->tableView->setCurrentIndex(index);
}

void QnPtzManageDialog::at_deleteButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid())
        return;

    QnPtzManageModel::RowData data = m_model->rowData(index.row());
    switch (data.rowType) {
    case QnPtzManageModel::PresetRow: {
        bool presetIsInUse = false;

        foreach (const QnPtzTourItemModel &tourModel, m_model->tourModels()) {
            foreach (const QnPtzTourSpot &spot, tourModel.tour.spots) {
                if (spot.presetId == data.presetModel.preset.id) {
                    presetIsInUse = true;
                    break;
                }
            }

            if (presetIsInUse)
                break;
        }

        if (presetIsInUse) {
            bool ignorePresetIsInUse = qnSettings->isPtzPresetInUseWarningDisabled();
            if (!ignorePresetIsInUse) {
                QDialogButtonBox::StandardButton button = QnCheckableMessageBox::warning(
                    this,
                    tr("Remove preset"),
                    tr("This preset is used in some tours.\nThese tours will become invalid if you remove it."),
                    tr("Do not show again."),
                    &ignorePresetIsInUse,
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                    QDialogButtonBox::Cancel
                );

                qnSettings->setPtzPresetInUseWarningDisabled(ignorePresetIsInUse);

                if (button == QDialogButtonBox::Cancel)
                    break;
            }
        }

        m_model->removePreset(data.presetModel.preset.id);
        break;
    }
    case QnPtzManageModel::TourRow:
        m_model->removeTour(data.tourModel.tour.id);
        break;
    default:
        break;
    }
}

void QnPtzManageDialog::at_getPreviewButton_clicked() {
    if (!controller() || !m_resource)
        return;

    QnPtzManageModel::RowData rowData = m_model->rowData(ui->tableView->currentIndex().row());
    if (rowData.rowType != QnPtzManageModel::PresetRow)
        return;

    QnPtzObject currentPosition;
    if (!controller()->getActiveObject(&currentPosition))
        return;

    if (currentPosition.id != rowData.presetModel.preset.id) {
        m_pendingPreviews.insert(rowData.presetModel.preset.id);
        controller()->activatePreset(rowData.presetModel.preset.id, 1.0);
        // when the preset will be activated a preview will be taken in updateFields()
    } else {
        storePreview(rowData.presetModel.preset.id);
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

void QnPtzManageDialog::at_cache_imageLoaded(const QString &fileName, bool ok) {
    if (!ok)
        return;

    QnPtzManageModel::RowData rowData = m_model->rowData(ui->tableView->currentIndex().row());
    if (rowData.id() != fileName)
        return;

    QnThreadedImageLoader *loader = new QnThreadedImageLoader(this);
    loader->setInput(m_cache->getFullPath(fileName));
    loader->setTransformationMode(Qt::FastTransformation);
    loader->setSize(ui->previewLabel->size());
    loader->setFlags(Qn::TouchSizeFromOutside);
    // TODO: #dklychkov rename QnThreadedImageLoader::finished() and use the new syntax
    connect(loader, SIGNAL(finished(QImage)), this, SLOT(setPreview(QImage)));
    loader->start();
}

void QnPtzManageDialog::storePreview(const QString &id) {
    QList<QnResourceWidget *> widgets = display()->widgets(m_resource);
    if (widgets.isEmpty())
        return;

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(widgets.first());
    if (!widget)
        return;

    // TODO: #dklychkov get image from the resource
//    widget->display()->mediaResource()->getImage(...);
    QImage image;
    m_cache->storeImage(id, image);
}

void QnPtzManageDialog::setPreview(const QImage &image) {
    if (image.isNull()) {
        ui->previewStackedWidget->setCurrentWidget(ui->noPreviewPage);
    } else {
        ui->previewStackedWidget->setCurrentWidget(ui->previewPage);
        ui->previewLabel->setPixmap(QPixmap::fromImage(image));
    }
}

void QnPtzManageDialog::at_model_modelAboutToBeReset() {
    QModelIndex index = ui->tableView->currentIndex();
    if (index.isValid()) {
        m_lastColumn = index.column();
        m_lastRowData = m_model->rowData(index.row());
    } else {
        m_lastColumn = -1;
        m_lastRowData.rowType = QnPtzManageModel::InvalidRow;
    }
}

void QnPtzManageDialog::at_model_modelReset() {
    if (!m_lastRowData.id().isEmpty()) {
        int row = m_model->rowNumber(m_lastRowData);
        if (row != -1) {
            QModelIndex index = m_model->index(row, m_lastColumn);

            QAbstractItemView::EditTriggers oldEditTriggers = ui->tableView->editTriggers();
            ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // to prevent field editor from showing
            ui->tableView->setCurrentIndex(index);
            ui->tableView->setEditTriggers(oldEditTriggers);
        }
    }
}

QnResourcePtr QnPtzManageDialog::resource() const {
    return m_resource;
}

void QnPtzManageDialog::setResource(const QnResourcePtr &resource) {
    if (m_resource == resource)
        return;
    
    m_resource = resource;
    m_adaptor->setResource(resource);

    setWindowTitle(tr("Manage PTZ for %1").arg(getResourceName(m_resource)));
}

bool QnPtzManageDialog::isModified() const {
    return !m_model->synchronized();
}

void QnPtzManageDialog::updateUi() {
    ui->addTourButton->setEnabled(!m_model->presetModels().isEmpty());

    QModelIndex index = ui->tableView->currentIndex();
    QnPtzManageModel::RowData selectedRow;
    if (index.isValid())
        selectedRow = m_model->rowData(index.row());

    bool isPreset = selectedRow.rowType == QnPtzManageModel::PresetRow;
    bool isTour = selectedRow.rowType == QnPtzManageModel::TourRow;
    bool isValidTour = isTour && m_model->tourIsValid(selectedRow.tourModel);

    ui->previewGroupBox->setEnabled(isPreset || isTour);
    if (ui->previewGroupBox->isEnabled())
        m_cache->downloadFile(selectedRow.id());
    ui->deleteButton->setEnabled(isPreset || isTour);
    ui->goToPositionButton->setEnabled(isPreset || (isTour && !selectedRow.tourModel.tour.spots.isEmpty()));
    ui->startTourButton->setEnabled(isValidTour && !selectedRow.tourModel.modified);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(isModified());
}

void QnPtzManageDialog::updateHotkeys() {
    m_model->setHotkeys(m_adaptor->value());
}

bool QnPtzManageDialog::tryClose(bool force) {
    if (!isModified() || force || isHidden()) {
        if (force)
            hide();
        return true;
    }

    show();
    QnMessageBox::StandardButton button = QnMessageBox::question(
        this, 
        0,
        tr("PTZ configuration is not saved"),
        tr("Changes are not saved. Do you want to save them?"),
        QnMessageBox::Yes | QnMessageBox::No | QnMessageBox::Cancel, 
        QnMessageBox::Yes);

    switch (button) {
    case QnMessageBox::Yes:
        saveChanges();
        return true;
    case QnMessageBox::Cancel:
        return false;
    default:
        return true;
    }
}
