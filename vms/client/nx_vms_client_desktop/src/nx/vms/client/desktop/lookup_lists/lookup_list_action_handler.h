// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class LookupListActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    LookupListActionHandler(QObject* parent = nullptr);
    virtual ~LookupListActionHandler() override;

private:
    void openLookupListsDialog();
    void openLookupListsManagementDialog();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};


} // namespace nx::vms::client::desktop
