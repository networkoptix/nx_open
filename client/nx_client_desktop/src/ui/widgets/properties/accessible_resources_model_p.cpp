#include "accessible_resources_model_p.h"

#include <set>

#include <client/client_globals.h>

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx/utils/string.h>

#include <ui/models/resource/resource_list_model.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/common/html.h>

namespace {

static const int kMaxResourcesInTooltip = 10;

static const QVector<int> kAccessibleResourcesModelSourceColumns =
    []()
    {
        QVector<int> sourceColumns(QnAccessibleResourcesModel::ColumnCount, -1);
        sourceColumns[QnAccessibleResourcesModel::NameColumn]  = QnResourceListModel::NameColumn;
        sourceColumns[QnAccessibleResourcesModel::CheckColumn] = QnResourceListModel::CheckColumn;
        return sourceColumns;
    }();

} // anonymous namespace


QnAccessibleResourcesModel::QnAccessibleResourcesModel(QObject* parent) :
    base_type(kAccessibleResourcesModelSourceColumns, parent),
    m_allChecked(false),
    m_subject()
{
    connect(resourceAccessProvider(), &QnResourceAccessProvider::accessChanged, this,
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

QVariant QnAccessibleResourcesModel::data(const QModelIndex& index, int role) const
{
    if (role == Qn::DisabledRole && m_allChecked)
        return false;

    switch (index.column())
    {
        case IndirectAccessColumn:
        {
            auto resource = index.sibling(index.row(), NameColumn).
                data(Qn::ResourceRole).value<QnResourcePtr>();

            switch (role)
            {
                case Qt::DecorationRole:
                {
                    const auto accessLevels = resourceAccessProvider()->accessLevels(m_subject,
                        resource);

                    if (accessLevels.contains(nx::core::access::Source::layout))
                        return qnResIconCache->icon(QnResourceIconCache::SharedLayout);

                    if (accessLevels.contains(nx::core::access::Source::videowall))
                        return qnResIconCache->icon(QnResourceIconCache::VideoWall);

                    return QVariant();
                }

                case Qt::ToolTipRole:
                {
                    QnResourceList providers;
                    resourceAccessProvider()->accessibleVia(m_subject, resource, &providers);
                    return getTooltip(providers);
                }

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

QString QnAccessibleResourcesModel::getTooltip(const QnResourceList& providers) const
{
    if (providers.empty())
        return QString();

    static const QString kSpacer = lit("<br>&nbsp;&nbsp;&nbsp;");

    QStringList sortedNames;
    for (const auto& resource: providers)
    {
        if (auto layout = resource.dynamicCast<QnLayoutResource>())
        {
            if (layout->isServiceLayout())
                continue;
        }

        sortedNames << resource->getName();
    }
    sortedNames.removeDuplicates();

    QStringList lines;
    lines << tr("Access granted by:");

    const auto maxTooltipLines = std::min(kMaxResourcesInTooltip, sortedNames.size());
    std::for_each(sortedNames.begin(), sortedNames.begin() + maxTooltipLines,
        [&lines](const QString& name)
        {
            lines << htmlBold(name);
        });

    const auto more = sortedNames.size() - kMaxResourcesInTooltip;
    if (more > 0)
        lines << tr("...and %n more", "", more);

    return lit("<div style='white-space: pre'>%1</div>").arg(lines.join(kSpacer));
}

void QnAccessibleResourcesModel::accessChanged()
{
    emit dataChanged(index(0, IndirectAccessColumn), index(rowCount() - 1, IndirectAccessColumn));
}


