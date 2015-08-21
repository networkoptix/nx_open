#include "ioport_settings_widget.h"
#include "ui_ioport_settings_widget.h"
#include "ui/models/ioports_view_model.h"
#include "ui/delegates/ioport_item_delegate.h"

namespace {
    const int headerSpacing = 2;
}

QnIOPortSettingsWidget::QnIOPortSettingsWidget(QWidget* parent /* = 0*/):
    base_type(parent)
    , ui(new Ui::QnIOPortSettingsWidget)
    , m_model(new QnIOPortsViewModel(this))
{
    ui->setupUi(this);
    ui->tableView->setModel(m_model);
    ui->tableView->setItemDelegate(new QnIOPortItemDelegate(this));  
    ui->tableView->horizontalHeader()->setVisible(true);
    ui->tableView->horizontalHeader()->setStretchLastSection(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QnIOPortsViewModel::NameColumn, QHeaderView::Stretch);

    connect(m_model, &QAbstractItemModel::dataChanged, this, &QnIOPortSettingsWidget::dataChanged);
}

QnIOPortSettingsWidget::~QnIOPortSettingsWidget() 
{ }

void QnIOPortSettingsWidget::updateHeaderWidth() 
{
    if (m_model->rowCount() == 0)
        return;

    for (int j = 0; j <= QnIOPortsViewModel::DefaultStateColumn; ++j)
    {
        QModelIndex idx = m_model->index(0, j);
        QScopedPointer<QWidget> widget(ui->tableView->itemDelegate()->createEditor(0, QStyleOptionViewItem(), idx));
        ui->tableView->horizontalHeader()->resizeSection(j, widget->minimumSizeHint().width());
    }

    QString text = m_model->headerData(QnIOPortsViewModel::AutoResetColumn, Qt::Horizontal).toString();
    QFontMetrics fm(ui->tableView->font());
    ui->tableView->horizontalHeader()->resizeSection(QnIOPortsViewModel::AutoResetColumn, fm.width(text) + headerSpacing * 2);
}

void QnIOPortSettingsWidget::updateFromResource(const QnVirtualCameraResourcePtr &camera)
{
    m_model->setModelData(camera ? camera->getIOPorts() : QnIOPortDataList());
    updateHeaderWidth();
}

void QnIOPortSettingsWidget::submitToResource(const QnVirtualCameraResourcePtr &camera)
{
    if (camera)
        camera->setIOPorts(m_model->modelData());
}
