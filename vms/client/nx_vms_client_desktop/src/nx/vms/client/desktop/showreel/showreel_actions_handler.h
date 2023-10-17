// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::api { struct ShowreelData; }

namespace nx::vms::client::desktop {

class ShowreelExecutor;
class ShowreelReviewController;

class ShowreelActionsHandler: public QObject, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    ShowreelActionsHandler(QObject* parent = nullptr);
    virtual ~ShowreelActionsHandler() override;

    virtual bool tryClose(bool force) override;

    QnUuid runningShowreel() const;

private:
    void saveShowreelToServer(const nx::vms::api::ShowreelData& showreel);
    void removeShowreelFromServer(const QnUuid& showreelId);

private:
    ShowreelExecutor* m_executor;
    ShowreelReviewController* m_reviewController;
};

} // namespace nx::vms::client::desktop
