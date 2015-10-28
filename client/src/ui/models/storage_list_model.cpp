#include "storage_list_model.h"

#include <core/resource/storage_resource.h>

namespace {
    const qreal BYTES_IN_GB = 1000000000.0;
}

// ------------------ QnStorageListMode --------------------------

QnStorageListModel::QnStorageListModel(QObject *parent)
    : base_type(parent)
    , m_isBackupRole(false)
    , m_readOnly(false)
    , m_linkBrush(QPalette().link())
{
    m_linkFont.setUnderline(true);
}

QnStorageListModel::~QnStorageListModel() {}

void QnStorageListModel::setStorages(const QnStorageSpaceDataList& storages)
{
    beginResetModel();
    m_storages = storages;
    sortStorages();
    endResetModel();
}

void QnStorageListModel::addStorage(const QnStorageSpaceData& data)
{
    beginResetModel();
    m_storages.push_back(data);
    sortStorages();
    endResetModel();
}


void QnStorageListModel::sortStorages() {
    qSort(m_storages.begin(), m_storages.end(), [](const QnStorageSpaceData &left, const QnStorageSpaceData &right) {
        
        /* Local storages should go first. */
        if (left.isExternal != right.isExternal)
            return right.isExternal;

        /* Group storages by plugin. */
        if (left.storageType != right.storageType)
            return QString::compare(left.storageType, right.storageType, Qt::CaseInsensitive) < 0;

        return QString::compare(left.url, right.url, Qt::CaseInsensitive) < 0;
    });
}


QnStorageSpaceDataList QnStorageListModel::storages() const
{
    return m_storages;
}

int QnStorageListModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return m_storages.size();
    return 0;
}

int QnStorageListModel::columnCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return ColumnCount;
    return 0;
}

QString urlPath(const QString& url)
{
    if (url.indexOf(lit("://")) > 0) {
        QUrl u(url);
        return u.host() + (u.port() != -1 ? lit(":").arg(u.port()) : lit(""))  + u.path();
    }
    else {
        return url;
    }
}

QString QnStorageListModel::displayData(const QModelIndex &index, bool forcedText) const
{
    const QnStorageSpaceData& storageData = m_storages[index.row()];
    switch (index.column())
    {
    case CheckBoxColumn:
        return QString();
    case UrlColumn:
        return urlPath(storageData.url);
    case TypeColumn:
        return storageData.storageType;
    case TotalSpaceColumn:
        switch (storageData.totalSpace) {
        case QnStorageResource::UnknownSize:
            return tr("Invalid storage");
        default:
            return tr("%1 Gb").arg(QString::number(storageData.totalSpace/BYTES_IN_GB, 'f', 1));
        }
    case RemoveActionColumn:
        return (storageData.isExternal || forcedText) && (!m_readOnly)
            ? tr("Remove") 
            : QString();

    case ChangeGroupActionColumn:
        if (m_readOnly)
            return QString();

        if (!storageData.isWritable)
            return tr("Inaccessible");

        return m_isBackupRole 
            ? tr("Use as main storage") 
            : canMoveStorage(storageData)
            ? tr("Use as backup storage")
            : QString();
    default:
        break;
    }

    return QString();
}

QVariant QnStorageListModel::fontData(const QModelIndex &index) const {
    if (m_readOnly)
        return QVariant();

    const QnStorageSpaceData& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return QVariant();

    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        return m_linkFont;

    return QVariant();
}

QVariant QnStorageListModel::foregroundData(const QModelIndex &index) const {
    if (m_readOnly)
        return QVariant();

    const QnStorageSpaceData& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return QVariant();

    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        return m_linkBrush;

    return QVariant();
}

QVariant QnStorageListModel::mouseCursorData(const QModelIndex &index) const {
    if (m_readOnly)
        return QVariant();

    const QnStorageSpaceData& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return QVariant();

    if (index.column() == RemoveActionColumn || index.column() == ChangeGroupActionColumn)
        if (!index.data(Qt::DisplayRole).toString().isEmpty())
            return QVariant::fromValue<int>(Qt::PointingHandCursor);

    return QVariant();
}

QVariant QnStorageListModel::checkstateData(const QModelIndex &index) const
{
    if (index.column() == CheckBoxColumn) {
        const QnStorageSpaceData& storageData = m_storages[index.row()];
        return storageData.isUsedForWriting && storageData.isWritable ? Qt::Checked : Qt::Unchecked;
    }
    return QVariant();
}

QVariant QnStorageListModel::data(const QModelIndex &index, int role) const 
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    switch(role) {
    case Qt::DisplayRole:
    case Qn::DisplayHtmlRole:
        return displayData(index, false);
    case Qn::TextWidthDataRole:
        return displayData(index, true);
    case Qt::FontRole:
        return fontData(index);
    case Qt::ForegroundRole:
        return foregroundData(index);
    case Qn::ItemMouseCursorRole:
        return mouseCursorData(index);
    case Qt::CheckStateRole:
        return checkstateData(index);
    case Qn::StorageSpaceDataRole:
        return QVariant::fromValue<QnStorageSpaceData>(m_storages[index.row()]);
    default:
        break;
    }
    return QVariant();

    return QVariant();
}

bool QnStorageListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.model() != this || !hasIndex(index.row(), index.column(), index.parent()) || m_readOnly)
        return false;

    QnStorageSpaceData& storageData = m_storages[index.row()];
    if (!storageData.isWritable)
        return false;

    if (role == Qt::CheckStateRole)
    {
        storageData.isUsedForWriting = (value == Qt::Checked);
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    else {
        return base_type::setData(index, value, role);
    }
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

bool QnStorageListModel::isBackupRole() const {
    return m_isBackupRole;
}

void QnStorageListModel::setBackupRole(bool value) {
    m_isBackupRole = value;
}

bool QnStorageListModel::isReadOnly() const {
    return m_readOnly;
}

void QnStorageListModel::setReadOnly( bool readOnly ) {
    if (m_readOnly == readOnly)
        return;

    beginResetModel();
    m_readOnly = readOnly;
    endResetModel();
}

bool QnStorageListModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid())
        return false;

    beginResetModel();
    m_storages.erase(m_storages.begin() + row, m_storages.begin() + row + count);
    endResetModel();
    return true;
}

bool QnStorageListModel::canMoveStorage( const QnStorageSpaceData& data ) const {
    if (!data.isWritable)
        return false;

    if (isBackupRole())
        return true;

    /* Check that at least one writable storage left in the main pool. */
    return std::any_of(m_storages.cbegin(), m_storages.cend(), [data](const QnStorageSpaceData &other) {

        /* Do not count subject to remove. */
        if (other.url == data.url)
            return false;

        if (!other.isWritable)
            return false;

        return true;
    });

}

