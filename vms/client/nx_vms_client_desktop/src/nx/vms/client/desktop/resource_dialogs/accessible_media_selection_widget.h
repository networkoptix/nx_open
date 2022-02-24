// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <business/business_resource_validation.h>

#include <nx/vms/client/desktop/resource_dialogs/resource_selection_widget.h>
#include <core/resource_access/resource_access_subject.h>

namespace nx::vms::client::desktop {

class IndirectAccessDecoratorModel;

class AccessibleMediaSelectionWidget: public ResourceSelectionWidget
{
    Q_OBJECT
    using base_type = ResourceSelectionWidget;

public:
    AccessibleMediaSelectionWidget(QWidget* parent);
    virtual ~AccessibleMediaSelectionWidget() override;

    void setSubject(const QnResourceAccessSubject& subject);

protected:
    virtual QAbstractItemModel* model() const override final;
    virtual void setupHeader() override;

private:
    std::unique_ptr<IndirectAccessDecoratorModel> m_indirectAccessDecoratorModel;
};

} // namespace nx::vms::client::desktop
