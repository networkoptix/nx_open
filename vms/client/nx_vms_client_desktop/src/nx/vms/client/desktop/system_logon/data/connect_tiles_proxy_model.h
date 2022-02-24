// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractProxyModel>

#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <ui/models/systems_model.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class SystemsVisibilitySortFilterModel;

/**
 * Provides "Connect to Cloud" and "Connect to System" tiles visibility logic for WelcomeScreen.
 * It's proxying the whole model as is, adding cloud tile as a first row and connect tile as a
 * last row.
 *
 * ConnectTilesProxyModel is used atop of SystemsVisibilitySortFilterModel.
 */
class NX_VMS_CLIENT_DESKTOP_API ConnectTilesProxyModel: public ScopedModelOperations<QAbstractProxyModel>
{
    Q_OBJECT
    Q_PROPERTY(bool loggedToCloud READ loggedToCloud WRITE setLoggedToCloud)
    Q_PROPERTY(QVariant visibilityFilter READ visibilityFilter WRITE setVisibilityFilter
        NOTIFY visibilityFilterChanged)
    /** Number of tiles, including connect tiles. */
    Q_PROPERTY(int systemsCount READ systemsCount NOTIFY systemsCountChanged)
    /** Total number of tiles, including filtered out systems and connect tiles. */
    Q_PROPERTY(int totalSystemsCount READ totalSystemsCount NOTIFY totalSystemsCountChanged)

    using base_type = ScopedModelOperations<QAbstractProxyModel>;

public:
    enum RoleId
    {
        FirstRoleId = QnSystemsModel::RolesCount,
        TileTypeRoleId = FirstRoleId,
        RolesCount
    };

    enum TileType
    {
        GeneralTileType,
        CloudButtonTileType,
        ConnectButtonTileType
    };
    Q_ENUM(TileType)

    using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;

public:
    ConnectTilesProxyModel(QObject* parent = nullptr);
    virtual ~ConnectTilesProxyModel() override;

    using CloudTileVisibilityScopeGetter = std::function<TileVisibilityScope()>;
    using CloudTileVisibilityScopeSetter = std::function<void(TileVisibilityScope)>;
    void setCloudVisibilityScopeCallbacks(
        CloudTileVisibilityScopeGetter getter, CloudTileVisibilityScopeSetter setter);

    // Setting source model to model other then SystemsVisibilitySortFilterModel
    // is not supported.
    virtual void setSourceModel(QAbstractItemModel* sourceModel) override;

public:
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qt::EditRole) override;

    QVariant visibilityFilter() const;
    void setVisibilityFilter(QVariant filter);

    int systemsCount() const;
    int totalSystemsCount() const;

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

public slots:
    bool loggedToCloud() const;
    void setLoggedToCloud(bool isLogged);

    Q_INVOKABLE void setFilterWildcard(const QString& filterWildcard);

signals:
    void visibilityFilterChanged();
    void systemsCountChanged();
    void totalSystemsCountChanged();

private:
    bool cloudTileIsVisible() const;

    /** Is cloud tile visibility scope satisfies current visibility filter value. */
    bool cloudTileIsAcceptedByFilter() const;

    /** Cloud tile can be only in "Default" or "Hidden" scopes. It cannot be in "Favorite". */
    bool isCloudTileVisibilityScopeHidden() const;

    void setCloudTileEnabled(bool enabled);
    void setConnectTileEnabled(bool enabled);

    bool isFirstIndex(const QModelIndex& index) const;
    bool isLastIndex(const QModelIndex& index) const;

    int mapRowToSource(int row) const;
    int mapRowFromSource(int row) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
