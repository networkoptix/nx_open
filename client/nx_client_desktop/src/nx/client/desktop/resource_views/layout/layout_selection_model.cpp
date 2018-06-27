#include "layout_selection_model.h"

#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_state.h>
#include <client_core/client_core_module.h>
#include <ui/style/resource_icon_cache.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>


namespace nx {
namespace client {
namespace desktop {

using State = LayoutSelectionDialogState;
using LayoutData = State::LayoutData;

struct LayoutsModel::Private
{
    State state;
    QHash<LayoutsModel::State::LayoutId, LayoutsModel::State::SubjectId> backRelations;

    QVariant getTextData(const QModelIndex& index) const;
    QVariant getCheckedData(const QModelIndex& index) const;
    QVariant getNodeIcon(const QModelIndex& index) const;

    bool isLayoutIndex(const QModelIndex& index) const;

private:
    QnUuid getLayoutId(const QModelIndex& index) const;
    QnUuid getSubjectId(const QModelIndex& index) const;
    LayoutData getLayoutData(const QModelIndex& index) const;
    QString getSubjectName(const QModelIndex& index) const;

};

bool LayoutsModel::Private::isLayoutIndex(const QModelIndex& index) const
{
    return state.flatView || index.parent().isValid();
}

QnUuid LayoutsModel::Private::getLayoutId(const QModelIndex& index) const
{
    if (!isLayoutIndex(index))
    {
        NX_EXPECT(false, "Index does not point to the layout!");
        return QnUuid();
    }

    const int row = index.row();
    const auto parentIndex = index.parent();
    if (parentIndex.isValid()) // It is layout
    {
        const auto subjectKey = state.subjects.keyAt(parentIndex.row());
        const auto& layoutIds = state.relations[subjectKey];
        return layoutIds[row];
    }

    if (state.flatView)
        return state.layouts.keyAt(row);

    NX_EXPECT(false, "Index does not point to the layout!");
    return QnUuid();

}

QnUuid LayoutsModel::Private::getSubjectId(const QModelIndex& index) const
{
    if (!isLayoutIndex(index))
        return state.subjects.keyAt(index.row());

    NX_EXPECT(false, "Index does not point to the subject!");
    return QnUuid();
}

LayoutData LayoutsModel::Private::getLayoutData(const QModelIndex& index) const
{
    const auto id = getLayoutId(index);
    if (!id.isNull())
        return state.layouts.value(id);

    NX_EXPECT(false, "Index does not point to the layout!");
    return LayoutData();
}

QString LayoutsModel::Private::getSubjectName(const QModelIndex& index) const
{
    const auto id = getSubjectId(index);
    if (!id.isNull())
        return state.subjects.value(id);

    NX_EXPECT(false, "Index does not point to subject!");
    return QString();
}

QVariant LayoutsModel::Private::getTextData(const QModelIndex& index) const
{
    return isLayoutIndex(index) ? getLayoutData(index).name : getSubjectName(index);
}

QVariant LayoutsModel::Private::getCheckedData(const QModelIndex& index) const
{
    if (index.column() != LayoutsModel::Columns::CheckMark)
        return Qt::Unchecked;

    if (isLayoutIndex(index))
        return getLayoutData(index).checked ? Qt::Checked : Qt::Unchecked;


    int checkedCount = 0;
    const auto subjectId = state.subjects.keyAt(index.row());
    const auto& layoutIds = state.relations[subjectId];
    for (const auto layoutId: layoutIds)
    {
        if (state.layouts.value(layoutId).checked)
            ++checkedCount;
    }
    if (!checkedCount)
        return Qt::Unchecked;

    return checkedCount == layoutIds.size() ? Qt::Checked : Qt::PartiallyChecked;
}

QVariant LayoutsModel::Private::getNodeIcon(const QModelIndex& index) const
{
    if (index.column() != Columns::Name)
        return QVariant();

    const auto resourceId = isLayoutIndex(index) ? getLayoutId(index) : getSubjectId(index);
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();
    return qnResIconCache->icon(pool->getResourceById(resourceId));
}

//-------------------------------------------------------------------------------------------------

LayoutsModel::LayoutsModel(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

LayoutsModel::~LayoutsModel()
{
}

void LayoutsModel::loadState(const State& state)
{
    ScopedReset resetGuard(this);
    d->state = state;

    auto& backRelations = d->backRelations;
    backRelations.clear();

    const auto& relations = state.relations;
    for (const auto& subjectId: relations.keys())
    {
        for (const auto& layoutId: relations.value(subjectId))
            backRelations.insert(layoutId, subjectId);
    }

    const auto& layouts = state.layouts;
    if (d->state.flatView)
    {
        insertRows(0, layouts.size());
        return;
    }

    int subjectRow = 0;
    for (const auto subjectData: state.subjects)
    {
        const auto layoutIds = relations.value(subjectData.key);
        if (layoutIds.isEmpty())
            continue;

        insertRow(subjectRow);

        const auto subjectIndex = index(subjectRow, Columns::Name);
        int layoutRow = 0;
        for (const auto& layoutId: layoutIds)
        {
            const auto it = layouts.find(layoutId);
            if (it != layouts.end())
                insertRow(layoutRow++, subjectIndex);
        }

        ++subjectRow;
    }
}

QModelIndex LayoutsModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    quintptr data = -1;
    if (!d->state.flatView && parent.isValid())
    {
        const auto subjectKey = d->state.subjects.keyAt(parent.row());
        const auto& layoutIds = d->state.relations[subjectKey];
        const auto layoutId = layoutIds.at(row);
        data = d->state.layouts.indexOfKey(layoutId);
    }

    return createIndex(row, column, data);
}

QModelIndex LayoutsModel::parent(const QModelIndex& child) const
{
    const int layoutIndex = child.internalId();
    if (layoutIndex == -1) // It is subject node or flat model.    
        return QModelIndex();

    const auto layoutId = d->state.layouts.keyAt(layoutIndex);
    const auto subjectId = d->backRelations.value(layoutId);
    const auto result = index(d->state.subjects.indexOfKey(subjectId), Columns::Name);
    return result;
}

int LayoutsModel::rowCount(const QModelIndex& node) const
{
    if (node.column() > Columns::Name)
        return 0;

    if (!node.isValid())
    {
        return d->state.flatView
            ? d->state.layouts.size() // We have only subjects here.
            : d->state.subjects.size(); // We have only layouts here.
    }

    // Returns layouts count for specified parent subject.
    if (const bool layoutNode = node.parent().isValid())
        return 0;

    const auto subjectKey = d->state.subjects.keyAt(node.row());
    return d->state.relations.value(subjectKey).size();
}

int LayoutsModel::columnCount(const QModelIndex& /* parent */) const
{
    return Columns::Count;
}

bool LayoutsModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

QVariant LayoutsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.column() > Columns::Name)
        return QVariant();

    switch(role)
    {
        case Qt::DisplayRole:
            return d->getTextData(index);
        case Qt::CheckStateRole:
            return d->getCheckedData(index);
        case Qt::DecorationRole:
            return d->getNodeIcon(index);
        default:
            return QVariant();
    }
    return QVariant();
}

Qt::ItemFlags LayoutsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return index.column() == Columns::CheckMark && d->isLayoutIndex(index)
        ? Qt::ItemIsSelectable
        : Qt::NoItemFlags;
}

} // namespace desktop
} // namespace client
} // namespace nx
