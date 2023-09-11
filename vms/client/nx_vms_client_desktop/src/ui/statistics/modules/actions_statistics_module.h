// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <statistics/abstract_statistics_module.h>
#include <statistics/base/base_fwd.h>

class AbstractMultimetric;

class QnActionsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnActionsStatisticsModule(QObject *parent);

    virtual ~QnActionsStatisticsModule();

    void setActionManager(const nx::vms::client::desktop::ui::action::ManagerPtr& manager);

    QnStatisticValuesHash values() const override;

    void reset() override;

private:
    typedef QList<QnStatisticsValuesProviderPtr> ModulesList;

    nx::vms::client::desktop::ui::action::ManagerPtr m_actionManager;
    ModulesList m_modules;
};
