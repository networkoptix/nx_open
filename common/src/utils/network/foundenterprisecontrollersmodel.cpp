/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#include "foundenterprisecontrollersmodel.h"

#include <algorithm>

#include <QtCore/QMutexLocker>

#include <utils/network/module_finder.h>

QnFoundEnterpriseControllersModel::QnFoundEnterpriseControllersModel(QnModuleFinder* const finder) {
    connect(finder,     &QnModuleFinder::moduleFound,   this,   &QnFoundEnterpriseControllersModel::remoteModuleFound,  Qt::DirectConnection);
    connect(finder,     &QnModuleFinder::moduleLost,    this,   &QnFoundEnterpriseControllersModel::remoteModuleLost,   Qt::DirectConnection);
}

//!Implementation of QAbstractItemModel::data
QVariant QnFoundEnterpriseControllersModel::data( const QModelIndex& index, int role ) const
{
    QMutexLocker lk( &m_mutex );

    if( index.column() != 0 )
        return QVariant();
    int foundModuleIndex = 0;
    int moduleAddressIndex = 0;
    if( index.internalId() == 0 )
    {
        if( index.row() >= m_foundModules.size() )
            return QVariant();
        foundModuleIndex = index.row();
        moduleAddressIndex = 0;
        if( role == Qt::DisplayRole )
            return getDisplayStringForEnterpriseControllerRootNode( m_foundModules[foundModuleIndex] );
    }
    else
    {
        foundModuleIndex = index.internalId() & 0x00ffffff;
        if( foundModuleIndex >= m_foundModules.size() ||
            index.row() >= m_foundModules[foundModuleIndex].ipAddresses.size() )   //invalid address index
        {
            return QVariant();
        }
        moduleAddressIndex = index.row();
        if( role == Qt::DisplayRole )
            return getDisplayStringForEnterpriseControllerAddressNode( m_foundModules[foundModuleIndex], m_foundModules[foundModuleIndex].ipAddresses[index.row()] );
    }
    const FoundModuleData& moduleData = m_foundModules[foundModuleIndex];

    switch( role )
    {
        case UrlRole:
            return QString::fromLatin1("https://%1:%2").arg(moduleData.ipAddresses[moduleAddressIndex]).arg(moduleData.port);

        case SeedRole:
            return moduleData.seed;

        case IpRole:
            return moduleData.ipAddresses[moduleAddressIndex];

        case PortRole:
            return moduleData.port;

        default:
            return QVariant();
    }
}

bool QnFoundEnterpriseControllersModel::hasChildren( const QModelIndex& parent ) const
{
    QMutexLocker lk( &m_mutex );

    if( !parent.isValid() )
        return true;

    if( parent.internalId() != 0 ||
        parent.column() > 0 ||
        parent.row() < 0 ||
        parent.row() >= m_foundModules.size() )
    {
        return false;
    }

    return m_foundModules[parent.row()].ipAddresses.size() > 1;
}

QVariant QnFoundEnterpriseControllersModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if( role != Qt::DisplayRole ||
        orientation != Qt::Horizontal ||
        section != 0 )
    {
        return QVariant();
    }

    return tr("Server addresses");
}

//!Implementation of QAbstractItemModel::index
QModelIndex	QnFoundEnterpriseControllersModel::index( int row, int column, const QModelIndex& parent ) const
{
    QMutexLocker lk( &m_mutex );

    return indexNonSafe( row, column, parent );
}

QModelIndex	QnFoundEnterpriseControllersModel::parent( const QModelIndex& index ) const
{
    if( index.column() != 0 || index.internalId() == 0 )
        return QModelIndex();

    const int foundModuleIndex = index.internalId() & 0x00ffffff;
    if( foundModuleIndex >= m_foundModules.size() )
        return QModelIndex();

    return createIndex( foundModuleIndex, 0, (void*)0 );
}

int QnFoundEnterpriseControllersModel::columnCount( const QModelIndex& index ) const
{
    Q_UNUSED(index)
    return 1;
}

//!Implementation of QAbstractItemModel::rowCount
int	QnFoundEnterpriseControllersModel::rowCount( const QModelIndex& parent ) const
{
    if( !parent.isValid() )
        return (int) m_foundModules.size();

    if( parent.internalId() != 0 || //element with ip address
        parent.column() != 0 ||
        parent.row() < 0 ||
        parent.row() >= m_foundModules.size() )
    {
        return 0;
    }

    //returning number of IPs of Server
    return (int) m_foundModules[parent.row()].ipAddresses.size();
}

void QnFoundEnterpriseControllersModel::remoteModuleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress) {
    QMutexLocker lk(&m_mutex);

    if (moduleInformation.type != nxMediaServerId)
        return;

    const QString &remoteHostAddress = moduleInformation.isLocal() ? QString::fromLatin1("127.0.0.1") : remoteAddress;
    const QString &url = QString(lit("https://%1:%2")).arg(remoteHostAddress).arg(moduleInformation.port);
    bool isNewElement = false;

    auto it = std::find_if(m_foundModules.begin(), m_foundModules.end(),
                           [&moduleInformation](const FoundModuleData &data) { return data.seed == moduleInformation.id.toString(); });
    if (it == m_foundModules.end()) {
        FoundModuleData newModuleData;
        newModuleData.url = url;
        newModuleData.seed = moduleInformation.id.toString();

        //searching place to insert new element in order of increase of url
        it = std::lower_bound(m_foundModules.begin(), m_foundModules.end(), newModuleData);
        if (it == m_foundModules.end() || it->url != newModuleData.url)
            it = m_foundModules.insert(it, newModuleData);

        isNewElement = true;
    }

    //if such already exists, updating it's data
    if (!it->ipAddresses.contains(remoteAddress))
        it->ipAddresses.append(remoteAddress);
    it->port = moduleInformation.port;

    const QModelIndex &updatedElemIndex = indexNonSafe(it - m_foundModules.begin(), 0);

    lk.unlock();

    if (isNewElement) {
        beginResetModel();
        endResetModel();
    } else {
        emit dataChanged(updatedElemIndex, updatedElemIndex);
    }
}

void QnFoundEnterpriseControllersModel::remoteModuleLost(const QnModuleInformation &moduleInformation) {
    QMutexLocker lk(&m_mutex);

    if (moduleInformation.type != nxMediaServerId)
        return;

    auto it = std::find_if(m_foundModules.begin(), m_foundModules.end(),
                           [&moduleInformation](const FoundModuleData &data) { return data.seed == moduleInformation.id.toString(); });
    if (it == m_foundModules.end())
        return;

    m_foundModules.erase(it);

    lk.unlock();

    beginResetModel();
    endResetModel();
}

QString QnFoundEnterpriseControllersModel::getDisplayStringForEnterpriseControllerRootNode( const FoundModuleData& moduleData ) const
{
    QString result;
    for(int i = 0; i < moduleData.ipAddresses.size(); ++i) {
        if(i > 0)
            result += lit(", ");
        result += lit("%1:%2").arg(moduleData.ipAddresses[i]).arg(moduleData.port);
    }
    return result;
}

QString QnFoundEnterpriseControllersModel::getDisplayStringForEnterpriseControllerAddressNode( const FoundModuleData& /*moduleData*/, const QString& address ) const
{
    return address;
}

QModelIndex	QnFoundEnterpriseControllersModel::indexNonSafe( int row, int column, const QModelIndex& parent ) const
{
    if( column != 0 )
        return QModelIndex();
    if( !parent.isValid() )
    {
        if( row >= m_foundModules.size() )
            return QModelIndex();
        return createIndex( row, column, (void*)0 );
    }
    else
    {
        if( parent.internalId() != 0 ||
            parent.row() >= m_foundModules.size() ||
            parent.column() > 0 )
        {
            return QModelIndex();
        }
        const FoundModuleData& moduleData = m_foundModules[parent.row()];
        if( row >= static_cast<int>(moduleData.ipAddresses.size()) )
            return QModelIndex();
        return createIndex( row, column, (1 << 24) | parent.row() ); //using non-zero internalId
    }
}
