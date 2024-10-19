// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>

#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/core/event_search/utils/analytics_search_setup.h>
#include <nx/vms/client/core/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::mobile {

class ParametersVisualizationModel: public ScopedModelOperations<QAbstractListModel>,
    public core::SystemContextAware
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

public:
    static void registerQmlType();

    ParametersVisualizationModel(QObject* parent = nullptr);
    virtual ~ParametersVisualizationModel() override;

    nx::vms::client::core::AnalyticsSearchSetup* analyticsSearchSetup() const;
    void setAnalyticsSearchSetup(nx::vms::client::core::AnalyticsSearchSetup* value);

    nx::vms::client::core::CommonObjectSearchSetup* searchSetup() const;
    void setSearchSetup(nx::vms::client::core::CommonObjectSearchSetup* value);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

signals:
    void analyticsSearchSetupChanged();
    void searchSetupChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
