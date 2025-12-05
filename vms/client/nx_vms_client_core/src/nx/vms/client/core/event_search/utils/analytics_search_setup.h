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
    Q_PROPERTY(nx::Uuid engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(QString objectType
        READ objectType WRITE setObjectType NOTIFY objectTypeChanged)

    Q_PROPERTY(QStringList attributeFilters READ attributeFilters WRITE setAttributeFilters
        NOTIFY attributeFiltersChanged)

    Q_PROPERTY(bool vectorizationSearchEnabled READ vectorizationSearchEnabled CONSTANT)

    Q_PROPERTY(analytics::db::TextScope textSearchScope READ textSearchScope WRITE setTextSearchScope
        NOTIFY textSearchScopeChanged)

    Q_PROPERTY(nx::Uuid referenceTrackId READ referenceTrackId WRITE setReferenceTrackId
        NOTIFY referenceTrackIdChanged)

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

    nx::Uuid engine() const;
    void setEngine(const nx::Uuid& value);

    QString objectType() const;
    void setObjectType(const QString& value);

    QStringList attributeFilters() const;
    void setAttributeFilters(const QStringList& value);
    QString combinedTextFilter() const;

    bool vectorizationSearchEnabled() const;

    nx::analytics::db::TextScope textSearchScope() const;
    void setTextSearchScope(nx::analytics::db::TextScope value);

    nx::Uuid referenceTrackId() const;
    void setReferenceTrackId(const nx::Uuid& value);

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
    void objectTypeChanged();
    void combinedTextFilterChanged();
    void textSearchScopeChanged();
    void referenceTrackIdChanged();
    void areaChanged();
    void areaEnabledChanged();
    void areaSelectionActiveChanged();
    void availableNewTracksChanged();
    void attributeFiltersChanged();
    void engineChanged();
    void ensureVisibleRequested(std::chrono::milliseconds timestamp, const nx::Uuid& trackId);
    void parametersChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
