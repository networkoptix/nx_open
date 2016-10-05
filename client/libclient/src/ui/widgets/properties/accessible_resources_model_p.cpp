#include "accessible_resources_model_p.h"

#include <set>

#include <client/client_globals.h>

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/string.h>

#include <ui/models/resource/resource_list_model.h>
#include <ui/style/resource_icon_cache.h>

namespace {


    // let code be here until it will be properly implemented
    /*
    const int kMaxResourcesInTooltip = 10;
    QString getTooltip()
    {
        int count = 0;
        QString tooltip;

        // Show only first kMaxResourcesInTooltip names from the sorted list:
        for (const auto& name : names)
        {
            if (++count > kMaxResourcesInTooltip)
                break;

            tooltip += lit("<br>&nbsp;&nbsp;&nbsp;%1").arg(name);
        }

        if (!tooltip.isEmpty())
        {
            QString suffix;
            if (count > kMaxResourcesInTooltip)
                suffix = lit("<br>&nbsp;&nbsp;&nbsp;%1").arg(tr("...and %n more", "", count - kMaxResourcesInTooltip));

            tooltip = lit("<div style='white-space: pre'>%1<b>%2</b>%3</div>").
                arg(tr("Access granted by:")).
                arg(tooltip).
                arg(suffix);
        }
    }
    */

} // anonymous namespace


QnAccessibleResourcesModel::QnAccessibleResourcesModel(QObject* parent) :
    base_type(parent),
    m_allChecked(false),
    m_subject()
{
    connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject)
        {
            if (subject == m_subject)
                accessChanged();
        });

}

QnAccessibleResourcesModel::~QnAccessibleResourcesModel()
{
}

QnResourceAccessSubject QnAccessibleResourcesModel::subject() const
{
    return m_subject;
}

void QnAccessibleResourcesModel::setSubject(const QnResourceAccessSubject& subject)
{
    if (m_subject == subject)
        return;
    m_subject = subject;
    accessChanged();
}

int QnAccessibleResourcesModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

QModelIndex QnAccessibleResourcesModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex QnAccessibleResourcesModel::sibling(int row, int column, const QModelIndex& idx) const
{
    return index(row, column, idx.parent());
}

QModelIndex QnAccessibleResourcesModel::parent(const QModelIndex& index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}

QModelIndex QnAccessibleResourcesModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();

    NX_ASSERT(proxyIndex.model() == this);
    switch (proxyIndex.column())
    {
        case NameColumn:
            return sourceModel()->index(proxyIndex.row(), QnResourceListModel::NameColumn);
        case CheckColumn:
            return sourceModel()->index(proxyIndex.row(), QnResourceListModel::CheckColumn);
        default:
            return QModelIndex();
    }
}

QModelIndex QnAccessibleResourcesModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();

    NX_ASSERT(sourceIndex.model() == sourceModel());
    switch (sourceIndex.column())
    {
        case QnResourceListModel::NameColumn:
            return createIndex(sourceIndex.row(), NameColumn);
        case QnResourceListModel::CheckColumn:
            return createIndex(sourceIndex.row(), CheckColumn);
        default:
            return QModelIndex();
    }
}

QVariant QnAccessibleResourcesModel::data(const QModelIndex& index, int role) const
{
    switch (index.column())
    {
        case IndirectAccessColumn:
        {
            switch (role)
            {
                case Qt::DecorationRole:
                {
                    auto resource = index.sibling(index.row(), NameColumn).
                        data(Qn::ResourceRole).value<QnResourcePtr>();
                    auto source = qnResourceAccessProvider->accessibleVia(m_subject, resource);
                    switch (source)
                    {
                        case QnAbstractResourceAccessProvider::Source::layout:
                            return qnResIconCache->icon(QnResourceIconCache::Layout);
                        case QnAbstractResourceAccessProvider::Source::videowall:
                            return qnResIconCache->icon(QnResourceIconCache::VideoWall);
                        default:
                            return QIcon();
                    }


                }

                //TODO: #GDM #implement me #high #3.0
                case Qt::ToolTipRole:
                    return lit("WILL_BE_FIXED_A_BIT_LATER");

                case Qn::DisabledRole:
                    return index.sibling(index.row(), NameColumn).data(role);

                default:
                    return QVariant();
            }
        }

        case CheckColumn:
        {
            if (m_allChecked && role == Qt::CheckStateRole)
                return QVariant::fromValue<int>(Qt::Checked);

            break;
        }

        default:
            break;
    }

    return base_type::data(index, role);
}

Qt::ItemFlags QnAccessibleResourcesModel::flags(const QModelIndex& index) const
{
    if (index.column() == IndirectAccessColumn)
        return base_type::flags(index.sibling(index.row(), NameColumn));

    return base_type::flags(index);
}

bool QnAccessibleResourcesModel::allChecked() const
{
    return m_allChecked;
}

void QnAccessibleResourcesModel::setAllChecked(bool value)
{
    if (m_allChecked == value)
        return;

    m_allChecked = value;

    emit dataChanged(
        index(0, CheckColumn),
        index(rowCount() - 1, CheckColumn),
        { Qt::CheckStateRole });
}

void QnAccessibleResourcesModel::accessChanged()
{
    emit dataChanged(index(0, IndirectAccessColumn), index(rowCount() - 1, IndirectAccessColumn));
}


