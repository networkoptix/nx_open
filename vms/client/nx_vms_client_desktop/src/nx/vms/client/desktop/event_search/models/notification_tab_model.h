// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/common/models/concatenation_list_model.h>

namespace nx::vms::client::desktop {

class WindowContext;

class NotificationTabModel: public core::ConcatenationListModel
{
    Q_OBJECT
    using base_type = core::ConcatenationListModel;

public:
    explicit NotificationTabModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~NotificationTabModel() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
