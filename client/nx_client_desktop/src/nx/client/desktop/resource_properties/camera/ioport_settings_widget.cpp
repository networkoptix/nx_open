#include "ioport_settings_widget.h"
#include "ui_ioport_settings_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <core/resource/param.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>

#include <ui/delegates/ioport_item_delegate.h>
#include <ui/graphics/items/overlays/io_module_overlay_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/ioports_view_model.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/common/widget_table.h>


namespace {

class QnIoPortSortedModel: public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel; //< forward constructor

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        /* Numbers have special sorting: */
        if (left.column() == QnIOPortsViewModel::NumberColumn)
            return left.row() < right.row();

        /* Durations are sorted by integer value: */
        if (left.column() == QnIOPortsViewModel::DurationColumn)
        {
            const auto leftValue = left.data(Qt::EditRole).toInt();
            const auto rightValue = right.data(Qt::EditRole).toInt();

            /* Zero (unset) durations are always last: */
            const bool leftZero = (leftValue == 0);
            const bool rightZero = (rightValue == 0);
            if (leftZero != rightZero)
                return rightZero;

            return leftValue < rightValue;
        }

        /* Other columns are sorted by string value: */
        const auto leftStr = left.data(Qt::DisplayRole).toString();
        const auto rightStr = right.data(Qt::DisplayRole).toString();

        /* Empty strings are always last: */
        if (leftStr.isEmpty() != rightStr.isEmpty())
            return rightStr.isEmpty();

        return nx::utils::naturalStringCompare(leftStr, rightStr, Qt::CaseInsensitive) < 0;
    }
};

} // namespace

namespace nx {
namespace client {
namespace desktop {

IoPortSettingsWidget::IoPortSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::IoPortSettingsWidget),
    m_model(new QnIOPortsViewModel(this))
{
    auto sortModel = new QnIoPortSortedModel(this);
    sortModel->setSourceModel(m_model);

    ui->setupUi(this);
    ui->table->setModel(sortModel);
    ui->table->setItemDelegate(new QnIoPortItemDelegate(this));
    ui->table->setSortingEnabled(true);
    ui->table->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->table->header()->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);
    ui->table->header()->setSortIndicator(QnIOPortsViewModel::NumberColumn, Qt::AscendingOrder);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(window());
    ui->table->setVerticalScrollBar(scrollBar->proxyScrollBar());

    // TODO: #vkutin #gdm #common Change to usual hasChanges/hasChangesChanged logic
    connect(m_model, &QAbstractItemModel::dataChanged,
        this, &IoPortSettingsWidget::dataChanged);
    connect(ui->enableTileInterfaceCheckBox, &QCheckBox::clicked,
        this, &IoPortSettingsWidget::dataChanged);

    setHelpTopic(this, Qn::IOModules_Help);
}

IoPortSettingsWidget::~IoPortSettingsWidget()
{
}

void IoPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr& camera)
{
    QnIOPortDataList ports;
    auto style = QnIoModuleOverlayWidget::Style::Default;

    if (camera)
    {
        ports = camera->getIOPorts();
        style = QnLexical::deserialized<QnIoModuleOverlayWidget::Style>(
            camera->getProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME),
            QnIoModuleOverlayWidget::Style::Default);
    }

    m_model->setModelData(ports);
    ui->enableTileInterfaceCheckBox->setCheckState(style == QnIoModuleOverlayWidget::Style::Tile
        ? Qt::Checked
        : Qt::Unchecked);
}

void IoPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    auto style = ui->enableTileInterfaceCheckBox->isChecked()
        ? QnIoModuleOverlayWidget::Style::Tile
        : QnIoModuleOverlayWidget::Style::Form;

    /* First, change style: */
    camera->setProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME, QnLexical::serialized(style));

    /* Second, change ports: */
    auto portList = m_model->modelData();
    if (!portList.empty()) //< can happen if it's just discovered unauthorized I/O module
        camera->setIOPorts(portList);
}

} // namespace desktop
} // namespace client
} // namespace nx
