#include "layout_selection_model.h"

#include <nx/client/desktop/resource_views/layout/layout_selection_dialog_state.h>

namespace {

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct LayoutsModel::Private
{
    LayoutsModel::State state;
    QHash<LayoutsModel::State::LayoutId, LayoutsModel::State::SubjectId> backRelations;
};

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

    if (role == Qt::CheckStateRole)
        return Qt::Checked;

    if (role != Qt::DisplayRole)
        return QVariant();


    const int row = index.row();
    const auto parentIndex = index.parent();
    if (parentIndex.isValid()) // It is layout
    {
        const auto subjectKey = d->state.subjects.keyAt(parentIndex.row());
        const auto& layoutIds = d->state.relations[subjectKey];
        const auto& layoutData = d->state.layouts.value(layoutIds[row]);
        return layoutData.name;
    }
    else if (d->state.flatView)
    {
        const auto layoutName = d->state.layouts.at(row).name;
        return layoutName;
    }
    else
    {
        const auto subjectName = d->state.subjects.at(row);
        return subjectName;
    }

    return QVariant();
}

} // namespace desktop
} // namespace client
} // namespace nx
