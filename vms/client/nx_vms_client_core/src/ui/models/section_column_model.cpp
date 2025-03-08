// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "section_column_model.h"

namespace nx::vms::client::core {

SectionColumnModel::~SectionColumnModel() {}

void SectionColumnModel::setSectionProperty(const QString& sectionProperty)
{
    if (m_sectionProperty == sectionProperty)
        return;

    m_sectionProperty = sectionProperty;
    emit sectionPropertyChanged();

    m_sectionRole = findSectionRole();

    updateModel();
}

int SectionColumnModel::findSectionRole() const
{
    if (!m_sourceModel)
        return -1;

    for (const auto& [role, roleName]: m_sourceModel->roleNames().asKeyValueRange())
    {
        if (roleName == m_sectionProperty)
            return role;
    }

    return -1;
}

void SectionColumnModel::setColumns(int columns)
{
    if (m_columns == columns)
        return;

    m_columns = columns;
    emit columnsChanged();

    updateModel();
}

void SectionColumnModel::updateModel()
{
    if (!sourceModel())
        return;

    auto newMapping = Mapping::fromModel(sourceModel(), m_sectionRole, m_columns);
    const int diff = newMapping.rowCount - m_mapping.rowCount;

    // This is potentially dangerous, as we send begin*Rows() signals when the source model has
    // already been changed, but it's still better than sending modelReset().

    if (diff > 0)
    {
        beginInsertRows({}, m_mapping.rowCount, m_mapping.rowCount + diff - 1);
        m_mapping = std::move(newMapping);
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, m_mapping.rowCount + diff, m_mapping.rowCount - 1);
        m_mapping = std::move(newMapping);
        endRemoveRows();
    }
    else
    {
        m_mapping = std::move(newMapping);
    }

    // Workaround for a bug in Qt: sometimes QML ListView does not update `count` property if
    // model's rowCount() jumps N -> 0 -> N.
    // Similar issue: https://bugreports.qt.io/browse/QTBUG-129165
    if (m_mapping.rowCount == 0)
    {
        beginResetModel();
        endResetModel();
        return;
    }

    if (m_mapping.rowCount > 0)
        emit dataChanged(index(0, 0), index(m_mapping.rowCount - 1, 0));
}

void SectionColumnModel::setSourceModel(QAbstractItemModel* model)
{
    if (m_sourceModel == model)
        return;

    if (sourceModel())
        disconnect(sourceModel(), nullptr, this, nullptr);

    beginResetModel();

    m_sectionRole = -1;

    m_sourceModel = model;

    if (model)
    {
        m_sectionRole = findSectionRole();

        connect(sourceModel(), &QAbstractItemModel::dataChanged, this,
            [this](
                const QModelIndex& topLeft,
                const QModelIndex& bottomRight,
                const QList<int>& roles)
            {
                if (roles.contains(m_sectionRole))
                {
                    updateModel();
                    return;
                }

                emit dataChanged(
                    index(m_mapping.fromSourceRow(topLeft.row()), 0),
                    index(m_mapping.fromSourceRow(bottomRight.row()), 0), {Qt::DisplayRole});
            });

        connect(sourceModel(), &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex& /*parent*/, int /*start*/, int /*end*/)
            {
                updateModel();
            });

        connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex& /*parent*/, int /*start*/, int /*end*/)
            {
                updateModel();
            });

        connect(sourceModel(), &QAbstractItemModel::modelAboutToBeReset, this,
            [this]()
            {
                beginResetModel();
                m_mapping.clear();
            });

        connect(sourceModel(), &QAbstractItemModel::modelReset, this,
            [this]()
            {
                updateSections();
                endResetModel();
            });

        connect(sourceModel(), &QAbstractItemModel::rowsMoved, this,
            [this]()
            {
                updateModel();
            });

        connect(sourceModel(), &QAbstractItemModel::layoutChanged, this,
            [this]()
            {
                updateModel();
            });
    }

    updateSections();
    endResetModel();

    emit sourceModelChanged();
}

QVariant SectionColumnModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (!m_sourceModel)
        return {};

    const int sourceRow = m_mapping.toSourceRow(index.row());
    if (sourceRow == -1)
        return {};

    if (role == SectionRole)
        return sourceModel()->index(sourceRow, 0).data(m_sectionRole);

    if (role != Qt::DisplayRole)
        return {};

    QVariantList result;

    for (int i = sourceRow; i < std::min(sourceRow + m_columns, sourceModel()->rowCount()); ++i)
    {
        if (index.row() != m_mapping.fromSourceRow(i))
            break;

        QVariantMap item;

        for (const auto& [role, roleName]: sourceModel()->roleNames().asKeyValueRange())
            item[roleName] = sourceModel()->index(i, 0).data(role);

        result.push_back(item);
    }

    return result;
}

QHash<int, QByteArray> SectionColumnModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SectionRole] = "section";
    roles[Qt::DisplayRole] = "data";
    return roles;
}

int SectionColumnModel::rowCount(const QModelIndex&) const
{
    return m_mapping.rowCount;
}

SectionColumnModel::Mapping SectionColumnModel::Mapping::fromModel(
    QAbstractItemModel* model,
    int sectionRole,
    int columns)
{
    Mapping result;

    result.columns = columns;

    const int sourceRowCount = model->rowCount();

    std::optional<QString> lastSection;

    for (int i = 0; i < sourceRowCount; ++i)
    {
        const auto index = model->index(i, 0);
        const auto section = index.data(sectionRole).toString();
        if (lastSection != section)
        {
            lastSection = section;

            const int lastSourceRow = result.sections.empty()
                ? 0
                : result.sections.back().startSourceRow;

            const int itemsInLastSection = i - lastSourceRow;
            result.rowCount += itemsInLastSection / result.columns
                + (itemsInLastSection % result.columns == 0 ? 0 : 1);

            result.sections.push_back(
                {
                    .startRow = result.rowCount,
                    .startSourceRow = i,
                    .section = section
                });
        }
    }

    // Count items in the last section.
    if (!result.sections.empty())
    {
        const int row = result.sections.back().startSourceRow;
        const int itemsInLastSection = sourceRowCount - row;
        result.rowCount += itemsInLastSection / result.columns
            + (itemsInLastSection % result.columns == 0 ? 0 : 1);
    }

    return result;
}

int SectionColumnModel::Mapping::toSourceRow(int row) const
{
    if (row < 0 || row >= rowCount || sections.empty())
        return -1;

    auto section = std::upper_bound(
        sections.begin(),
        sections.end(),
        row,
        [](int row, const Section& section)
        {
            return row < section.startRow;
        });

    if (section == sections.begin())
        return -1;

    --section;

    return section->startSourceRow + (row - section->startRow) * columns;
}

int SectionColumnModel::Mapping::fromSourceRow(int sourceRow) const
{
    if (sourceRow < 0)
        return -1;

    auto section = std::upper_bound(
        sections.begin(),
        sections.end(),
        sourceRow,
        [](int row, const Section& section)
        {
            return row < section.startSourceRow;
        });

    if (section == sections.begin())
        return -1;

    --section;

    return section->startRow + (sourceRow - section->startSourceRow) / columns;
}

void SectionColumnModel::updateSections()
{
    m_sectionRole = findSectionRole();

    if (!m_sourceModel || m_sectionRole == -1)
    {
        m_mapping.clear();
        return;
    }

    m_mapping = Mapping::fromModel(m_sourceModel, m_sectionRole, m_columns);
}

} // namespace nx::vms::client::core
