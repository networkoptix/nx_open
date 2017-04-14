#include "layout_tour_dialog.h"
#include "ui_layout_tour_dialog.h"

#include <QtWidgets/QStyledItemDelegate>

#include <nx_ec/data/api_layout_tour_data.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/layout_tour_item.h>
#include <core/resource/layout_resource.h>

#include <ui/models/layout_tour_items_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/views/resource_list_view.h>

namespace {

static const int kDefaultDelayMs = 5 * 1000; // 5 seconds

QnLayoutResourcePtr selectLayout(QnResourcePool* resourcePool)
{
    auto layouts = resourcePool->getResources<QnLayoutResource>();

    QScopedPointer<QnMessageBox> dialog(new QnMessageBox());
    auto resourcesView = new QnResourceListView(layouts, dialog.data());
    resourcesView->setSelectionEnabled(true);
    dialog->setText(QnLayoutTourDialog::tr("Select layout..."));
    dialog->addCustomWidget(resourcesView);
    dialog->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    if (!dialog->exec())
        return QnLayoutResourcePtr();

    return resourcesView->selectedResource().dynamicCast<QnLayoutResource>();
}

class DelayEditDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;
public:
    DelayEditDelegate(QObject* parent = nullptr):
        base_type(parent)
    {}

protected:
    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
        const QModelIndex& index) const override
    {
        NX_ASSERT(index.column() == QnLayoutTourItemsModel::DelayColumn);
        auto spinBox = new QSpinBox(parent);
        spinBox->setMinimum(1);
        spinBox->setMaximum(9999);
        return spinBox;
    }

    virtual void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto spinBox = qobject_cast<QSpinBox*>(editor))
            spinBox->setValue(index.data(Qt::EditRole).toInt() / 1000);
    }

    virtual void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override
    {
        if (auto spinBox = qobject_cast<QSpinBox*>(editor))
            model->setData(index, spinBox->value() * 1000);
    }
};

} // namespace

QnLayoutTourDialog::QnLayoutTourDialog(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::LayoutTourDialog),
    m_model(new QnLayoutTourItemsModel(this))
{
    ui->setupUi(this);

    ui->nameInputField->setTitle(tr("Name"));

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->layoutsTreeView->setVerticalScrollBar(scrollBar->proxyScrollBar());

    ui->layoutsTreeView->setModel(m_model);
    ui->layoutsTreeView->setItemDelegateForColumn(QnLayoutTourItemsModel::DelayColumn,
        new DelayEditDelegate(this));
    ui->layoutsTreeView->setProperty(style::Properties::kSuppressHoverPropery, true);
    ui->layoutsTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->layoutsTreeView->header()->setSectionResizeMode(QnLayoutTourItemsModel::ControlsColumn,
        QHeaderView::Stretch);

    setSafeMinimumSize(QSize(600, 400));
    setResizeToContentsMode(Qt::Horizontal | Qt::Vertical);

    connect(ui->layoutsTreeView, &QTreeView::clicked, this, &QnLayoutTourDialog::at_view_clicked);
    connect(ui->addLayoutButton, &QPushButton::clicked, this,
        [this]
        {
            if (auto layout = selectLayout(resourcePool()))
                m_model->addItem(QnLayoutTourItem(layout, kDefaultDelayMs));
        });
}

QnLayoutTourDialog::~QnLayoutTourDialog()
{
}

void QnLayoutTourDialog::loadData(const ec2::ApiLayoutTourData& tour)
{
    ui->nameInputField->setText(tour.name);
    m_model->reset(QnLayoutTourItem::createList(tour.items, resourcePool()));
}

void QnLayoutTourDialog::submitData(ec2::ApiLayoutTourData* tour) const
{
    NX_ASSERT(tour);
    if (!tour)
        return;

    tour->name = ui->nameInputField->text();
    tour->items.clear();
    for (const auto& item: m_model->items())
    {
        if (!item.layout)
            continue;
        tour->items.emplace_back(item.layout->getId(), item.delayMs);
    }
}

void QnLayoutTourDialog::at_view_clicked(const QModelIndex& index)
{
    auto column = static_cast<QnLayoutTourItemsModel::Column>(index.column());
    switch (column)
    {
        case QnLayoutTourItemsModel::LayoutColumn:
            if (auto layout = selectLayout(resourcePool()))
            {
                m_model->updateLayout(index.row(), layout);
            }
            break;
        case QnLayoutTourItemsModel::MoveUpColumn:
            m_model->moveUp(index.row());
            break;
        case QnLayoutTourItemsModel::MoveDownColumn:
            m_model->moveDown(index.row());
            break;
        case QnLayoutTourItemsModel::ControlsColumn:
            m_model->removeItem(index.row());
            break;
        default:
            break;
    }
}
