// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <analytics/db/analytics_db_types.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::core { class AnalyticsSearchSetup; }

namespace nx::vms::client::desktop {

class WindowContext;

/**
 * An utility class to synchronize Right Panel analytics tab state with current media widget
 * analytics selection mode and analytics chunks display on the timeline.
 */
class AnalyticsSearchSynchronizer: public AbstractSearchSynchronizer
{
    using base_type = AbstractSearchSynchronizer;
    Q_OBJECT

public:
    AnalyticsSearchSynchronizer(
        WindowContext* windowContext,
        CommonObjectSearchSetup* commonSetup,
        core::AnalyticsSearchSetup* analyticsSetup,
        QObject* parent = nullptr);

    virtual ~AnalyticsSearchSynchronizer() override;

    void ensureVisible(std::chrono::milliseconds timestamp, const nx::Uuid& trackId);

private:
    bool calculateMediaResourceWidgetAnalyticsEnabled(QnMediaResourceWidget* widget) const;

    void updateAreaSelection();
    void updateWorkbench();
    void updateAction();
    void updateCachedDevices();
    void updateMediaResourceWidgetAnalyticsMode(QnMediaResourceWidget* widget);
    void updateAllMediaResourceWidgetsAnalyticsMode();
    void updateObjectType(const QStringList& objectTypeIds);
    void updateCameraSelection(core::EventSearch::CameraSelection cameraSelection);

    void handleWidgetAnalyticsFilterRectChanged();

    void setAreaSelectionActive(bool value);

    bool isMasterInstance() const;
    void setupInstanceSynchronization();
    QVector<AnalyticsSearchSynchronizer*> instancesToNotify();
    static QVector<AnalyticsSearchSynchronizer*>& instances();

private:
    const QPointer<core::AnalyticsSearchSetup> m_analyticsSetup;
    nx::utils::ScopedConnections m_activeMediaWidgetConnections;
    bool m_areaSelectionActive = false;
    bool m_updating = false;
    nx::analytics::db::Filter m_filter;
    core::analytics::taxonomy::AnalyticsFilterModel* m_filterModel = nullptr;
};

} // namespace nx::vms::client::desktop
