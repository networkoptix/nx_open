// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace nx::vms::client::desktop {

class CheckableHeaderView;

class UserListWidget:
    public Connective<QnAbstractPreferencesWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QnAbstractPreferencesWidget>;

public:
    explicit UserListWidget(QWidget* parent = nullptr);
    virtual ~UserListWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;

    void filterDigestUsers();

private:
    class Private;
    class Delegate;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
