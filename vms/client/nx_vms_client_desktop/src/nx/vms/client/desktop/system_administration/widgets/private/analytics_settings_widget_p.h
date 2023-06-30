// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>
#include <QtQuickWidgets/QQuickWidget>

#include <api/server_rest_connection.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/system_administration/models/api_integration_requests_model.h>

#include "../analytics_settings_widget.h"

namespace nx::vms::client::desktop {

class AnalyticsSettingsWidget::Private:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    Q_PROPERTY(QVariant analyticsEngines READ analyticsEngines NOTIFY analyticsEnginesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit Private(AnalyticsSettingsWidget* q);

    Q_INVOKABLE QnUuid getCurrentEngineId() const { return currentEngineId; }
    Q_INVOKABLE void setCurrentEngineId(const QnUuid& engineId);

    Q_INVOKABLE QJsonObject settingsValues(const QnUuid& engineId);

    Q_INVOKABLE QVariant requestParameters(const QJsonObject& model);

    Q_INVOKABLE void setSettingsValues(
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& values,
        const QJsonObject& parameters);

    Q_INVOKABLE QJsonObject settingsModel(const QnUuid& engineId);
    Q_INVOKABLE QJsonObject errors(const QnUuid& engineId) { return m_errors[engineId]; }

    QVariant analyticsEngines() const { return engines; }

    void updateEngines();
    void activateEngine(const QnUuid& engineId);

    bool loading() const { return settingsLoading; }
    void setLoading(bool loading);

    void refreshSettings(const QnUuid& engineId);
    void activeElementChanged(
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& parameters);

    void applySettingsValues();

    void updateHasChanges();

    Q_INVOKABLE ApiIntegrationRequestsModel* makeApiIntegrationRequestsModel() const;

public:
    struct SettingsValues
    {
        QJsonObject values;
        bool changed = false;
    };

public:
    AnalyticsSettingsWidget* const q = nullptr;

    const QScopedPointer<QQuickWidget> view;
    const QScopedPointer<core::AnalyticsEnginesWatcher> enginesWatcher;
    QVariantList engines;
    bool hasChanges = false;
    bool settingsLoading = false;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;

    QnUuid currentEngineId;

    QHash<QnUuid, QJsonObject> m_errors;
    QHash<QnUuid, SettingsValues> settingsValuesByEngineId;
    QHash<QnUuid, QJsonObject> settingsModelByEngineId;
    QVariant requests;

signals:
    void analyticsEnginesChanged();
    void currentSettingsStateChanged();
    void currentErrorsChanged();
    void loadingChanged();
    void licenseSummariesChanged();

private:
    void addEngine(const QnUuid& engineId, const core::AnalyticsEngineInfo& engineInfo);
    void removeEngine(const QnUuid& engineId);
    void updateEngine(const QnUuid& engineId);
    void setErrors(const QnUuid& engineId, const QJsonObject& errors);
    void resetSettings(
        const QnUuid& engineId,
        const QJsonObject& model,
        const QJsonObject& values,
        const QJsonObject& errors,
        bool changed);
};

} // namespace nx::vms::client::desktop
