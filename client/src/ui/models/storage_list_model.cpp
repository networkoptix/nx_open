#include "storage_list_model.h"
#include <ui/workbench/workbench_access_controller.h>

namespace {
    const qreal BYTES_IN_GB = 1000000000.0;
}

// ------------------ QnStorageListMode --------------------------

QnStorageListModel::QnStorageListModel(QObject *parent)
    : base_type(parent)
    , m_isBackupRole(false)
    , m_linkBrush(QPalette().link())
{
    m_linkFont.setUnderline(true);
}

QnStorageListModel::~QnStorageListModel() {}

void QnStorageListModel::setModelData(const QnStorageSpaceReply& data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

int QnStorageListModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_data.storages.size();
    return 0;
}

int QnStorageListModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QString urlPath(const QString& url)
{
    if (url.indexOf(lit("://")) > 0)
        return QUrl(url).path().mid(1);
    else
        return url;
}

QString QnStorageListModel::displayData(const QModelIndex &index) const
{
    const QnStorageSpaceData& storageData = m_data.storages[index.row()];
    switch (index.column())
    {
    case CheckBoxColumn:
        return QString();
    case UrlColumn:
        return urlPath(storageData.url);
    case TypeColumn:
        return storageData.storageType;
    case TotalSpaceColumn:
        return QString::number(storageData.totalSpace/BYTES_IN_GB, 'f', 1) + tr("Gb");
    case RemoveActionColumn:
        return storageData.isExternal ? tr("Remove") : QString();
    case ChangeGroupActionColumn:
        return m_isBackupRole ? tr("Use as main storage") : tr("Use as backup storage");
    default:
        break;
    }

    return QString();
}

QVariant QnStorageListModel::fontData(const QModelIndex &index) const 
{
    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        return m_linkFont;

    return QVariant();
}

QVariant QnStorageListModel::foregroundData(const QModelIndex &index) const 
{
    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        return m_linkBrush;
    return QVariant();
}

QVariant QnStorageListModel::mouseCursorData(const QModelIndex &index) const
{
    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        return QVariant::fromValue<int>(Qt::PointingHandCursor);
    return QVariant();
}

QVariant QnStorageListModel::checkstateData(const QModelIndex &index) const
{
    const QnStorageSpaceData& storageData = m_data.storages[index.row()];
    if (index.column() == CheckBoxColumn)
        return storageData.isUsedForWriting ? Qt::Checked : Qt::Unchecked;
    return QVariant();
}

QVariant QnStorageListModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch(role) {
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return displayData(index);
    case Qt::FontRole:
        return fontData(index);
    case Qt::ForegroundRole:
        return foregroundData(index);
    case Qn::ItemMouseCursorRole:
        return mouseCursorData(index);
    case Qt::CheckStateRole:
        return checkstateData(index);
    default:
        break;
    }
    return QVariant();

    return QVariant();
}

QVariant QnStorageListModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    // column headers aren't used so far
    return base_type::headerData(section, orientation, role);
}

Qt::ItemFlags QnStorageListModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::NoItemFlags;
    flags |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == CheckBoxColumn)
        flags |= Qt::ItemIsUserCheckable;

    return flags;
}

void QnStorageListModel::setBackupRole(bool value)
{
    m_isBackupRole = value;
}
