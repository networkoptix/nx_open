// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/vms/client/desktop/resource_dialogs/detailed_resource_tree_widget.h>

#include <nx/vms/api/types/resource_types.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class FailoverPriorityDecoratorModel;

class FailoverPriorityViewWidget: public DetailedResourceTreeWidget
{
    Q_OBJECT
    using base_type = DetailedResourceTreeWidget;
    using FailoverPriorityMap = QHash<QnVirtualCameraResourcePtr, nx::vms::api::FailoverPriority>;

public:
    FailoverPriorityViewWidget(QWidget* parent);
    virtual ~FailoverPriorityViewWidget() override;

    FailoverPriorityMap modifiedFailoverPriority() const;

protected:
    virtual QAbstractItemModel* model() const override;

private:
    std::unique_ptr<FailoverPriorityDecoratorModel> m_failoverPriorityDecoratorModel;
};

} // namespace nx::vms::client::desktop
