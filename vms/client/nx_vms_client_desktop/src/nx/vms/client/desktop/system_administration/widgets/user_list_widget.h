// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class UserListWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit UserListWidget(QWidget* parent = nullptr);
    virtual ~UserListWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual bool hasChanges() const override;
    virtual void resetWarnings() override;

    void filterDigestUsers();

    virtual QSize sizeHint() const override;

private:
    class Private;
    class Delegate;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
