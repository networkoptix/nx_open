// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_view_widget.h"

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_tree_entity_builder.h>
#include <nx/vms/client/desktop/resource_dialogs/models/failover_priority_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/details/filtered_resource_view_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/item_delegates/failover_priority_dialog_item_delegate.h>
#include <nx/vms/client/desktop/resource_dialogs/details/failover_priority_picker_widget.h>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>

namespace {

static constexpr auto kViewItemIndents = QnIndents(nx::style::Metrics::kDefaultTopLevelMargin);

using AbstractEntityPtr = nx::vms::client::desktop::entity_item_model::AbstractEntityPtr;
using ResourceTreeEntityBuilder =
    nx::vms::client::desktop::entity_resource_tree::ResourceTreeEntityBuilder;

static constexpr int kColumnCount = 1;

AbstractEntityPtr createTreeEntity(const ResourceTreeEntityBuilder* builder)
{
    return builder->createDialogAllCamerasEntity(
        /*showServers*/ true,
        std::function<bool(const QnResourcePtr&)>());
}

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

FailoverPriorityViewWidget::FailoverPriorityViewWidget(QWidget* parent):
    base_type(kColumnCount, parent),
    m_failoverPriorityDecoratorModel(new FailoverPriorityDecoratorModel())
{
    auto failoverPriorityPicker = new FailoverPriorityPickerWidget();
    resourceViewWidget()->setFooterWidget(failoverPriorityPicker);
    failoverPriorityPicker->switchToPlaceholder();

    connect(resourceViewWidget(), &FilteredResourceViewWidget::selectionChanged, this,
        [this, failoverPriorityPicker]()
        {
            if (resourceViewWidget()->selectedIndexes().isEmpty())
                failoverPriorityPicker->switchToPlaceholder();
            else
                failoverPriorityPicker->switchToPicker();
        });

    connect(failoverPriorityPicker, &FailoverPriorityPickerWidget::failoverPriorityClicked, this,
        [this](FailoverPriority failoverPriority)
        {
            m_failoverPriorityDecoratorModel->setFailoverPriorityOverride(
                resourceViewWidget()->selectedIndexes(), failoverPriority);
        });

    resourceViewWidget()->setSelectionMode(QAbstractItemView::ExtendedSelection);
    resourceViewWidget()->setItemDelegate(new FailoverPriorityDialogItemDelegate(this));
    resourceViewWidget()->setSideIndentation(kViewItemIndents);
    resourceViewWidget()->setVisibleItemPredicate(
        [](const QModelIndex& index)
        {
            if (const auto parentIndex = index.parent(); parentIndex.isValid())
            {
                const auto parentResource =
                    parentIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
                if (parentResource && parentResource->hasFlags(Qn::server))
                    return true;
            }
            return false;
        });

    setTreeEntityFactoryFunction(createTreeEntity);
}

FailoverPriorityViewWidget::~FailoverPriorityViewWidget()
{
}

FailoverPriorityViewWidget::FailoverPriorityMap
    FailoverPriorityViewWidget::modifiedFailoverPriority() const
{
    return m_failoverPriorityDecoratorModel->failoverPriorityOverride();
}

QAbstractItemModel* FailoverPriorityViewWidget::model() const
{
    if (!m_failoverPriorityDecoratorModel->sourceModel())
        m_failoverPriorityDecoratorModel->setSourceModel(base_type::model());

    return m_failoverPriorityDecoratorModel.get();
}

} // namespace nx::vms::client::desktop
