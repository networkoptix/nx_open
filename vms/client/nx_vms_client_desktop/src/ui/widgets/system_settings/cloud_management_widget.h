// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class CloudManagementWidget; }

namespace nx::vms::client::desktop{ class ConnectToCloudTool; }

class QnCloudManagementWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnCloudManagementWidget(QWidget *parent = nullptr);
    virtual ~QnCloudManagementWidget();

    virtual void loadDataToUi() override;
    virtual bool isNetworkRequestRunning() const override;
    virtual void discardChanges() override;

private:
    void connectToCloud();
    void disconnectFromCloud();
    bool confirmCloudDisconnect();
    void onDisconnectSuccess();

private:
    QScopedPointer<Ui::CloudManagementWidget> ui;
    QPointer<nx::vms::client::desktop::ConnectToCloudTool> m_connectTool;
    rest::Handle m_currentRequest = 0;
};
