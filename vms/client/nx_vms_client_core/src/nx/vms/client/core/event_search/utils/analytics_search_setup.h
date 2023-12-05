// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/event_search/models/analytics_search_list_model.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AnalyticsSearchSetup: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnUuid engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(QStringList objectTypes
        READ objectTypes WRITE setObjectTypes NOTIFY objectTypesChanged)

    Q_PROPERTY(QStringList attributeFilters READ attributeFilters WRITE setAttributeFilters
        NOTIFY attributeFiltersChanged)

    Q_PROPERTY(QRectF area READ area WRITE setArea NOTIFY areaChanged)
    Q_PROPERTY(bool isAreaSelected READ isAreaSelected NOTIFY areaChanged)
    Q_PROPERTY(bool areaEnabled READ areaEnabled NOTIFY areaEnabledChanged)

    Q_PROPERTY(bool areaSelectionActive READ areaSelectionActive WRITE setAreaSelectionActive
        NOTIFY areaSelectionActiveChanged)

    Q_PROPERTY(int availableNewTracks READ availableNewTracks NOTIFY availableNewTracksChanged)

public:
    static void registerQmlType();

    explicit AnalyticsSearchSetup(AnalyticsSearchListModel* model, QObject* parent = nullptr);
    virtual ~AnalyticsSearchSetup() override;

    QnUuid engine() const;
    void setEngine(const QnUuid& value);

    QStringList objectTypes() const;
    void setObjectTypes(const QStringList& value);
    const std::set<QString>& relevantObjectTypes() const;

    QStringList attributeFilters() const;
    void setAttributeFilters(const QStringList& value);
    QString combinedTextFilter() const;

    QRectF area() const;
    void setArea(const QRectF& value);
    bool isAreaSelected() const;

    bool areaEnabled() const;

    bool areaSelectionActive() const;
    void setAreaSelectionActive(bool value);

    Q_INVOKABLE void clearAreaSelection();

    AnalyticsSearchListModel* model() const;

    void setLiveTimestampGetter(AnalyticsSearchListModel::LiveTimestampGetter value);

    int availableNewTracks() const;
    Q_INVOKABLE void commitAvailableNewTracks();

signals:
    void objectTypesChanged();
    void relevantObjectTypesChanged();
    void combinedTextFilterChanged();
    void areaChanged();
    void areaEnabledChanged();
    void areaSelectionActiveChanged();
    void availableNewTracksChanged();
    void attributeFiltersChanged();
    void engineChanged();
    void ensureVisibleRequested(std::chrono::milliseconds timestamp, const QnUuid& trackId,
        const QnTimePeriod& proposedTimeWindow);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
