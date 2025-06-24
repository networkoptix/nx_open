// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/mobile/window_context_aware.h>

Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/analytics_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/bookmark_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/common_object_search_setup.h")

namespace nx::vms::client::core {
class AnalyticsSearchSetup;
class BookmarkSearchSetup;
class CommonObjectSearchSetup;
} // namespace nx::vms::client::core

namespace nx::vms::client::mobile {

class ParametersVisualizationModel: public ScopedModelOperations<QAbstractListModel>,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

    Q_PROPERTY(nx::vms::client::core::AnalyticsSearchSetup* analyticsSearchSetup
            READ analyticsSearchSetup
            WRITE setAnalyticsSearchSetup
            NOTIFY analyticsSearchSetupChanged)

    Q_PROPERTY(nx::vms::client::core::CommonObjectSearchSetup* searchSetup
        READ searchSetup
        WRITE setSearchSetup
        NOTIFY searchSetupChanged)

    Q_PROPERTY(nx::vms::client::core::BookmarkSearchSetup* bookmarkSearchSetup
        READ bookmarkSearchSetup
        WRITE setBookmarkSearchSetup
        NOTIFY bookmarkSearchSetupChanged)

public:
    static void registerQmlType();

    ParametersVisualizationModel(QObject* parent = nullptr);
    virtual ~ParametersVisualizationModel() override;

    core::AnalyticsSearchSetup* analyticsSearchSetup() const;
    void setAnalyticsSearchSetup(core::AnalyticsSearchSetup* value);

    core::CommonObjectSearchSetup* searchSetup() const;
    void setSearchSetup(core::CommonObjectSearchSetup* value);

    core::BookmarkSearchSetup* bookmarkSearchSetup() const;
    void setBookmarkSearchSetup(core::BookmarkSearchSetup* value);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void analyticsSearchSetupChanged();
    void searchSetupChanged();
    void bookmarkSearchSetupChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
