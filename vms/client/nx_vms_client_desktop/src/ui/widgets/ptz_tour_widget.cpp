// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_tour_widget.h"
#include "ui_ptz_tour_widget.h"

#include <client/client_globals.h>
#include <core/ptz/ptz_tour.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <ui/delegates/ptz_tour_item_delegate.h>
#include <ui/models/ptz_tour_spots_model.h>
#include <utils/common/event_processors.h>

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconTheme = {
    {QnIcon::Normal, {.primary = "light10"}},
};

NX_DECLARE_COLORIZED_ICON(kAddIcon, "20x20/Outline/add.svg", kIconTheme)
NX_DECLARE_COLORIZED_ICON(kMinusIcon, "20x20/Outline/minus.svg", kIconTheme)
NX_DECLARE_COLORIZED_ICON(kArrowUpIcon, "20x20/Outline/arrow_up.svg", kIconTheme)
NX_DECLARE_COLORIZED_ICON(kArrowDownIcon, "20x20/Outline/arrow_down.svg", kIconTheme)

} // namespace

QnPtzTourWidget::QnPtzTourWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::PtzTourWidget),
    m_model(new QnPtzTourSpotsModel(this))
{
    ui->setupUi(this);

    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnPtzTourSpotsModel::NameColumn, QHeaderView::Stretch);

    ui->tableView->setItemDelegate(new QnPtzTourItemDelegate(this));
    ui->tableView->setCurrentIndex(QModelIndex());

    installEventHandler(ui->tableView->viewport(), QEvent::Resize, this,
        &QnPtzTourWidget::at_tableViewport_resizeEvent, Qt::QueuedConnection);

    connect(m_model, &QnPtzTourSpotsModel::spotsChanged, this, &QnPtzTourWidget::tourSpotsChanged);

    ui->addSpotButton->setIcon(qnSkin->icon(kAddIcon));
    connect(ui->addSpotButton, &QPushButton::clicked, this,
        &QnPtzTourWidget::at_addSpotButton_clicked);

    ui->deleteSpotButton->setIcon(qnSkin->icon(kMinusIcon));
    connect(ui->deleteSpotButton, &QPushButton::clicked, this,
        &QnPtzTourWidget::at_deleteSpotButton_clicked);

    ui->moveSpotUpButton->setIcon(qnSkin->icon(kArrowUpIcon));
    connect(ui->moveSpotUpButton, &QPushButton::clicked, this,
        &QnPtzTourWidget::at_moveSpotUpButton_clicked);

    ui->moveSpotDownButton->setIcon(qnSkin->icon(kArrowDownIcon));
    connect(ui->moveSpotDownButton, &QPushButton::clicked, this,
        &QnPtzTourWidget::at_moveSpotDownButton_clicked);
}

QnPtzTourWidget::~QnPtzTourWidget() {
}


const QnPtzTourSpotList& QnPtzTourWidget::spots() const {
    return m_model->spots();
}

void QnPtzTourWidget::setSpots(const QnPtzTourSpotList &spots) {
    m_model->setSpots(spots);
    if (!spots.isEmpty())
        ui->tableView->setCurrentIndex(ui->tableView->model()->index(0, 0));
}

const QnPtzPresetList& QnPtzTourWidget::presets() const {
    return m_model->presets();
}

void QnPtzTourWidget::setPresets(const QnPtzPresetList &presets) {
    m_model->setPresets(presets);
}

QnPtzTourSpot QnPtzTourWidget::currentTourSpot() const {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid())
        return QnPtzTourSpot();

    return m_model->data(index, Qn::PtzTourSpotRole).value<QnPtzTourSpot>();
}

void QnPtzTourWidget::at_addSpotButton_clicked() {
    m_model->insertRow(m_model->rowCount());

    ui->tableView->setCurrentIndex(m_model->index(m_model->rowCount() - 1, 0));

    for(int i = 0; i < ui->tableView->horizontalHeader()->count(); i++)
        if(ui->tableView->horizontalHeader()->sectionResizeMode(i) == QHeaderView::ResizeToContents)
            ui->tableView->resizeColumnToContents(i);
}

void QnPtzTourWidget::at_deleteSpotButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid())
        return;

    m_model->removeRow(index.row());
}

void QnPtzTourWidget::at_moveSpotUpButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid() || index.row() == 0)
        return;

    m_model->moveRow(index.parent(), index.row(), index.parent(), index.row() - 1);
}

void QnPtzTourWidget::at_moveSpotDownButton_clicked() {
    QModelIndex index = ui->tableView->currentIndex();
    if (!index.isValid() || index.row() == m_model->rowCount() - 1)
        return;

    m_model->moveRow(index.parent(), index.row(), index.parent(), index.row() + 2); // that is not an error, see QAbstractItemModel docs
}

void QnPtzTourWidget::at_tableViewport_resizeEvent() {
    QModelIndexList selectedIndices = ui->tableView->selectionModel()->selectedRows();
    if(selectedIndices.isEmpty())
        return;

    ui->tableView->scrollTo(selectedIndices.front());
}
