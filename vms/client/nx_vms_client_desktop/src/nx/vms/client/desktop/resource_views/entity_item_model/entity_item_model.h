// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QPointer>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Model which designed to be versatile, efficient and stable when it comes to resources
 * representation. Canonical model is the data source, this time is expected that data is
 * held by another sources, model role is to be an aggregator / proxy / facade. Only data it
 * has is cached data and nothing unique.
 */
class NX_VMS_CLIENT_DESKTOP_API EntityItemModel: public QAbstractItemModel
{
    Q_OBJECT
    using base_type = QAbstractItemModel;
    friend class EntityModelMapping;

public:
    /**
     * @param columnCount Column count which be in model being created. All data provided by
     *     attached entity will be mapped to the column 0, while all other columns will be blank.
     *     Creating model with column count > 1 has no meaning if it will be be used as is, but
     *     it's very convenient way to create source for QIdentityProxyModel-derived decorators,
     *     which may perform data remapping or provide custom data for initially blank columns.
     *     Overhead for mapping source model index to proxy model index and vice versa will be
     *     negligible. There is no way to change column count later.
     */
    EntityItemModel(int columnCount = 1);
    virtual ~EntityItemModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override final;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override final;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override final;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override final;

    virtual QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override final;

    virtual QModelIndex parent(const QModelIndex& index) const override final;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const override final;
    virtual QModelIndex sibling(int row, int column, const QModelIndex &index) const override final;

    /**
     * Overrides QAbstractItemModel::roleNames().
     * @note Regarding Resource Tree application: model should return names for all roles in use
     *     in predefined format to make QML view work correct.
     * And since role set is global, it's right place to do it in the base class, not only for the
     * resource tree, rather for all possible applications.
     */
    virtual QHash<int, QByteArray> roleNames() const override;

public:

    /**
     * Sets entity which will be data source for the model. Entity instance lifespan should be
     * inside model instance lifespan scope. If entity is already set to another model instance,
     * it will be transferred. Using single entity as data source for multiple models isn't
     * implemented yet.
     * @param entity Valid pointer to the entity or nullptr, if it's intended to reset model to
     *     initial state.
     */
    void setRootEntity(AbstractEntity* entity);

public:
    /**
     * See QAbstractItemModel::headerData(), please implement desired functionality via
     * QIdentityProxyModel derived decorator to keep things modular and reusable.
     * @returns Default constructed QVariant.
     */
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override final;

    /**
    * See QAbstractItemModel::setHeaderData(), please implement desired functionality via
    * QIdentityProxyModel derived decorator to keep things modular and reusable.
    * @returns False
    */
    virtual bool setHeaderData(int section, Qt::Orientation orientation,
        const QVariant& value, int role = Qt::EditRole) override final;

public:
    /**
     * Sets function wrapper which will be called from setSata() method with the same
     * index and data parameters for Qt::EditRole.
     */
    using EditDelegate = std::function<bool(const QModelIndex&, const QVariant&)>;
    void setEditDelegate(EditDelegate editDelegate);

    virtual bool setData(const QModelIndex&, const QVariant&, int = Qt::EditRole) override final;
    virtual bool setItemData(const QModelIndex&, const QMap<int, QVariant>&) override final;

    virtual bool insertColumns(int, int, const QModelIndex& = QModelIndex()) override final;
    virtual bool moveColumns(const QModelIndex&, int, int, const QModelIndex&, int) override final;
    virtual bool removeColumns(int, int, const QModelIndex& = QModelIndex()) override final;

    virtual bool insertRows(int, int, const QModelIndex& = QModelIndex()) override final;
    virtual bool moveRows(const QModelIndex&, int, int, const QModelIndex&, int) override final;
    virtual bool removeRows(int, int, const QModelIndex& = QModelIndex()) override final;

private:
    const int m_columnCount = 1;
    AbstractEntity* m_rootEntity = nullptr;
    EditDelegate m_editDelegate;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
