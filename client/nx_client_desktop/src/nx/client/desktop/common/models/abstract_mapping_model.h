#pragma once

#include <type_traits>

#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {
namespace desktop {

//-------------------------------------------------------------------------------------------------
// AbstractMultiMappingModel - a base for proxy models with multiple source models.

template<class Base>
class AbstractMultiMappingModel: public Base
{
    static_assert(std::is_base_of<QAbstractItemModel, Base>::value,
        "Base must be derived from QAbstractItemModel");

    using base_type = Base;

public:
    using base_type::base_type; //< Forward constructors.

    virtual QModelIndex mapToSource(const QModelIndex& index) const = 0;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const = 0;

public:
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QMap<int, QVariant> itemData(const QModelIndex& index) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual bool setData(const QModelIndex& index,
        const QVariant& value, int role = Qt::EditRole) override;
    virtual bool setItemData(const QModelIndex& index, const QMap<int, QVariant>& values) override;

    virtual QModelIndex buddy(const QModelIndex& index) const override;
    virtual QSize span(const QModelIndex& index) const override;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex sibling(int row, int column, const QModelIndex& index) const override;

protected:
    using PersistentIndexPair = QPair<
        QPersistentModelIndex, //< Proxy index.
        QPersistentModelIndex>; //< Source index.
};

//-------------------------------------------------------------------------------------------------
// AbstractMappingModel - a base for proxy models with one source model.
// More flexible than QAbstractProxyModel as it can be derived from any base.

template<class Base>
class AbstractMappingModel: public AbstractMultiMappingModel<Base>
{
    using base_type = AbstractMultiMappingModel<Base>;

public:
    using base_type::base_type; //< Forward constructors.

    virtual QAbstractItemModel* sourceModel() const = 0;

public:
    virtual QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    virtual bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value,
        int role = Qt::EditRole) override;

    virtual bool submit() override;
    virtual void revert() override;

    virtual bool canFetchMore(const QModelIndex& parent) const override;
    virtual void fetchMore(const QModelIndex& parent) override;

    virtual QHash<int, QByteArray> roleNames() const override;

    virtual QMimeData* mimeData(const QModelIndexList& indices) const override;
    virtual QStringList mimeTypes() const override;
    virtual Qt::DropActions supportedDragActions() const override;
    virtual Qt::DropActions supportedDropActions() const override;
    virtual bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) const override;
    virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) override;
private:
    void mapDropCoordinatesToSource(int row, int column, const QModelIndex& parent,
        int& sourceRow, int& sourceColumn, QModelIndex& sourceParent) const;
};

//-------------------------------------------------------------------------------------------------
// AbstractMultiMappingModel implementation.

template<class Base>
QVariant AbstractMultiMappingModel<Base>::data(const QModelIndex& index, int role) const
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? sourceIndex.model()->data(sourceIndex, role)
        : QVariant();
}

template<class Base>
QMap<int, QVariant> AbstractMultiMappingModel<Base>::itemData(const QModelIndex& index) const
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? sourceIndex.model()->itemData(sourceIndex)
        : QMap<int, QVariant>();
}

template<class Base>
Qt::ItemFlags AbstractMultiMappingModel<Base>::flags(const QModelIndex& index) const
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? sourceIndex.model()->flags(sourceIndex)
        : Qt::ItemFlags();
}

template<class Base>
bool AbstractMultiMappingModel<Base>::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? const_cast<QAbstractItemModel*>(sourceIndex.model())->setData(sourceIndex, value, role)
        : false;
}

template<class Base>
bool AbstractMultiMappingModel<Base>::setItemData(const QModelIndex& index,
    const QMap<int, QVariant>& values)
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? const_cast<QAbstractItemModel*>(sourceIndex.model())->setItemData(sourceIndex, values)
        : false;
}

template<class Base>
QModelIndex AbstractMultiMappingModel<Base>::buddy(const QModelIndex& index) const
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? mapFromSource(sourceIndex.model()->buddy(sourceIndex))
        : QModelIndex();
}

template<class Base>
QSize AbstractMultiMappingModel<Base>::span(const QModelIndex& index) const
{
    const auto sourceIndex = mapToSource(index);
    return sourceIndex.model()
        ? sourceIndex.model()->span(sourceIndex)
        : QSize();
}

template<class Base>
bool AbstractMultiMappingModel<Base>::hasChildren(const QModelIndex& parent) const
{
    const auto sourceParent = mapToSource(parent);
    return sourceParent.model()
        ? sourceParent.model()->hasChildren(sourceParent)
        : false;
}

template<class Base>
QModelIndex AbstractMultiMappingModel<Base>::sibling(int row, int column, const QModelIndex& index) const
{
    return this->index(row, column, index.parent());
}

//-------------------------------------------------------------------------------------------------
// AbstractMappingModel implementation.

template<class Base>
QVariant AbstractMappingModel<Base>::headerData(int section,
    Qt::Orientation orientation, int role) const
{
    return sourceModel()
        ? sourceModel()->headerData(section, orientation, role)
        : QVariant();
}

template<class Base>
bool AbstractMappingModel<Base>::setHeaderData(int section, Qt::Orientation orientation,
    const QVariant& value, int role)
{
    return sourceModel()
        ? sourceModel()->setHeaderData(section, orientation, value, role)
        : false;
}

template<class Base>
bool AbstractMappingModel<Base>::submit()
{
    return sourceModel() ? sourceModel()->submit() : false;
}

template<class Base>
void AbstractMappingModel<Base>::revert()
{
    if (sourceModel())
        sourceModel()->revert();
}

template<class Base>
bool AbstractMappingModel<Base>::canFetchMore(const QModelIndex& parent) const
{
    return sourceModel() ? sourceModel()->canFetchMore(this->mapToSource(parent)) : false;
}

template<class Base>
void AbstractMappingModel<Base>::fetchMore(const QModelIndex& parent)
{
    if (sourceModel())
        sourceModel()->fetchMore(this->mapToSource(parent));
}

template<class Base>
QHash<int, QByteArray> AbstractMappingModel<Base>::roleNames() const
{
    return sourceModel() ? sourceModel()->roleNames() : QHash<int, QByteArray>();
}

template<class Base>
QMimeData* AbstractMappingModel<Base>::mimeData(const QModelIndexList& indices) const
{
    if (!sourceModel())
        return nullptr;

    QModelIndexList mappedIndices;
    mappedIndices.reserve(indices.count());
    for (const auto& index: indices)
        mappedIndices << this->mapToSource(index);

    return sourceModel()->mimeData(mappedIndices);
}

template<class Base>
QStringList AbstractMappingModel<Base>::mimeTypes() const
{
    return sourceModel() ? sourceModel()->mimeTypes() : QStringList();
}

template<class Base>
Qt::DropActions AbstractMappingModel<Base>::supportedDragActions() const
{
    return sourceModel() ? sourceModel()->supportedDragActions() : Qt::DropActions();
}

template<class Base>
Qt::DropActions AbstractMappingModel<Base>::supportedDropActions() const
{
    return sourceModel() ? sourceModel()->supportedDropActions() : Qt::DropActions();
}

template<class Base>
bool AbstractMappingModel<Base>::canDropMimeData(const QMimeData* data, Qt::DropAction action,
    int row, int column, const QModelIndex& parent) const
{
    if (!sourceModel())
        return false;
    int sourceRow = -1;
    int sourceColumn = -1;
    QModelIndex sourceParent;
    mapDropCoordinatesToSource(row, column, parent, sourceRow, sourceColumn, sourceParent);
    return sourceModel()->canDropMimeData(data, action, sourceRow, sourceColumn, sourceParent);
}

template<class Base>
bool AbstractMappingModel<Base>::dropMimeData(const QMimeData* data, Qt::DropAction action,
    int row, int column, const QModelIndex& parent)
{
    if (!sourceModel())
        return false;
    int sourceRow = -1;
    int sourceColumn = -1;
    QModelIndex sourceParent;
    mapDropCoordinatesToSource(row, column, parent, sourceRow, sourceColumn, sourceParent);
    return sourceModel()->dropMimeData(data, action, sourceRow, sourceColumn, sourceParent);
}

template<class Base>
void AbstractMappingModel<Base>::mapDropCoordinatesToSource(int row, int column,
    const QModelIndex& parent, int& sourceRow, int& sourceColumn, QModelIndex& sourceParent) const
{
    sourceRow = -1;
    sourceColumn = -1;

    if (row == -1 && column == -1)
    {
        sourceParent = this->mapToSource(parent);
    }
    else if (row == this->rowCount(parent))
    {
        sourceParent = this->mapToSource(parent);
        sourceRow = sourceModel()->rowCount(sourceParent);
    }
    else
    {
        const auto proxyIndex = this->index(row, column, parent);
        const auto sourceIndex = this->mapToSource(proxyIndex);
        sourceRow = sourceIndex.row();
        sourceColumn = sourceIndex.column();
        sourceParent = sourceIndex.parent();
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
