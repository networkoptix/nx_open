// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>

#include <api/server_rest_connection_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/desktop/common/utils/engine_license_summary_provider.h>
#include <nx/vms/client/desktop/current_system_context_aware.h>
#include <nx/vms/client/desktop/system_administration/models/api_integration_requests_model.h>
#include <nx/vms/common/saas/saas_service_manager.h>

namespace nx::vms::client::desktop {

class AnalyticsSettingsStore:
    public QObject,
    public CurrentSystemContextAware
{
    Q_OBJECT
    Q_PROPERTY(QVariant analyticsEngines READ analyticsEngines NOTIFY analyticsEnginesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)
    Q_PROPERTY(bool isNewRequestsEnabled
        MEMBER m_isNewRequestsEnabled NOTIFY isNewRequestsEnabledChanged)
public:
    AnalyticsSettingsStore(QWidget* parent = nullptr);

    Q_INVOKABLE nx::Uuid getCurrentEngineId() const { return m_currentEngineId; }
    Q_INVOKABLE void setCurrentEngineId(const nx::Uuid& engineId);

    Q_INVOKABLE QJsonObject settingsValues(const nx::Uuid& engineId);

    Q_INVOKABLE QVariant requestParameters(const QJsonObject& model);

    Q_INVOKABLE void setSettingsValues(
        const nx::Uuid& engineId,
        const QString& activeElement,
        const QJsonObject& values,
        const QJsonObject& parameters);

    Q_INVOKABLE QJsonObject settingsModel(const nx::Uuid& engineId);
    Q_INVOKABLE QJsonObject errors(const nx::Uuid& engineId) { return m_errors[engineId]; }

    QVariant analyticsEngines() const { return m_engines; }
    bool isEmpty() const { return m_engines.empty(); }

    void updateEngines();

    bool loading() const { return m_settingsLoading; }
    void setLoading(bool loading);

    void refreshSettings(const nx::Uuid& engineId);
    void activeElementChanged(
        const nx::Uuid& engineId,
        const QString& activeElement,
        const QJsonObject& parameters);

    bool hasChanges() const { return m_hasChanges; }
    void updateHasChanges();
    bool isNetworkRequestRunning() const;

    Q_INVOKABLE void applySettingsValues();
    Q_INVOKABLE void applySystemSettings();
    Q_INVOKABLE void discardChanges();

    Q_INVOKABLE ApiIntegrationRequestsModel* makeApiIntegrationRequestsModel() const;
    Q_INVOKABLE EngineLicenseSummaryProvider* makeEngineLicenseSummaryProvider() const;
    Q_INVOKABLE nx::vms::common::saas::ServiceManager* saasServiceManager() const;

signals:
    void analyticsEnginesChanged();
    void currentSettingsStateChanged();
    void currentErrorsChanged();
    void loadingChanged();
    void licenseSummariesChanged();
    void isNewRequestsEnabledChanged();
    void hasChangesChanged();

private:
    struct SettingsValues
    {
        QJsonObject values;
        bool changed = false;
    };

private:
    void addEngine(const nx::Uuid& engineId, const core::AnalyticsEngineInfo& engineInfo);
    void removeEngine(const nx::Uuid& engineId);
    void updateEngine(const nx::Uuid& engineId);
    void setErrors(const nx::Uuid& engineId, const QJsonObject& errors);
    void resetSettings(
        const nx::Uuid& engineId,
        const QJsonObject& model,
        const QJsonObject& values,
        const QJsonObject& errors,
        bool changed);

private:
    QPointer<QWidget> m_parent;
    const QScopedPointer<core::AnalyticsEnginesWatcher> m_enginesWatcher;
    QVariantList m_engines;
    bool m_isNewRequestsEnabled = true;
    bool m_hasChanges = false;
    bool m_settingsLoading = false;
    QList<rest::Handle> m_pendingRefreshRequests;
    QList<rest::Handle> m_pendingApplyRequests;
    rest::Handle m_systemSettingsRequest = 0;

    nx::Uuid m_currentEngineId;

    QHash<nx::Uuid, QJsonObject> m_errors;
    QHash<nx::Uuid, SettingsValues> m_settingsValuesByEngineId;
    QHash<nx::Uuid, QJsonObject> m_settingsModelByEngineId;
};

} // namespace nx::vms::client::desktop
