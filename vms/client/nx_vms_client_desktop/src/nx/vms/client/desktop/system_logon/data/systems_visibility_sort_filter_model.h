// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <nx/vms/client/core/enums.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * Provides tiles visibility logic for WelcomeScreen.
 * The logic is based on QnSystemsModel::VisibilityScopeRoleId filtering.
 * VisibilityScope is stored by SystemsVisibilityManager.
 *
 * SystemsVisibilitySortFilterModel is used atop of QnSystemsModel.
 */
class NX_VMS_CLIENT_DESKTOP_API SystemsVisibilitySortFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT

    /** Total number of systems, including filtered out systems. */
    Q_PROPERTY(int totalSystemsCount READ totalSystemsCount NOTIFY totalSystemsCountChanged)

    using base_type = QSortFilterProxyModel;

public:
    using Filter = nx::vms::client::core::welcome_screen::TileScopeFilter;
    using CoreEnums = nx::vms::client::core::Enums;

public:
    SystemsVisibilitySortFilterModel(QObject* parent = nullptr);
    virtual ~SystemsVisibilitySortFilterModel() override;

    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;

    using VisibilityScopeGetter = std::function<Filter()>;
    using VisibilityScopeSetter = std::function<void(Filter)>;
    void setVisibilityScopeFilterCallbacks(
        VisibilityScopeGetter getter, VisibilityScopeSetter setter);

    using ForgottenCheckFunction = std::function<bool(const QString&)>;
    void setForgottenCheckCallback(ForgottenCheckFunction callback);

    void forgottenSystemAdded(const QString& systemId);
    void forgottenSystemRemoved(const QString& systemId);

public:
    Filter visibilityFilter() const;
    void setVisibilityFilter(Filter filter);

    int totalSystemsCount() const;

signals:
    void visibilityFilterChanged();
    void totalSystemsCountChanged();

private:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    virtual bool lessThan(const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;
    bool isHidden(const QModelIndex& sourceIndex) const;
    int forgottenSystemsCount(int firstRow, int lastRow) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
