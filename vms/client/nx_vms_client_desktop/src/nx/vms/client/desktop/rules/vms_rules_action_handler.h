// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::rules {

class VmsRulesActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit VmsRulesActionHandler(QObject* parent = nullptr);
    ~VmsRulesActionHandler() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    void openVmsRulesDialog();
    void openEventLogDialog();
};

} // namespace nx::vms::client::desktop::rules
