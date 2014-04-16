/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#include "foundenterprisecontrollersmodel.h"

#include <algorithm>

#include <QtCore/QMutexLocker>


using namespace std;

FoundEnterpriseControllersModel::FoundEnterpriseControllersModel( NetworkOptixModuleFinder* const finder )
{
    QObject::connect(
        finder,
        SIGNAL(moduleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)),
        this,
        SLOT(remoteModuleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, bool, const QString&)),
        Qt::DirectConnection );
    QObject::connect(
        finder,
        SIGNAL(moduleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)),
        this,
        SLOT(remoteModuleLost(const QString&, const TypeSpecificParamMap&, const QString&, bool, const QString&)),
        Qt::DirectConnection );
}

//!Implementation of QAbstractItemModel::data
QVariant FoundEnterpriseControllersModel::data( const QModelIndex& index, int role ) const
{
    QMutexLocker lk( &m_mutex );

    if( index.column() != 0 )
        return QVariant();
    size_t foundModuleIndex = 0;
    size_t moduleAddressIndex = 0;
    if( index.internalId() == 0 )
    {
        if( index.row() >= static_cast<int>(m_foundModules.size()) )
            return QVariant();
        foundModuleIndex = index.row();
        moduleAddressIndex = 0;
        if( role == Qt::DisplayRole )
            return getDisplayStringForEnterpriseControllerRootNode( m_foundModules[foundModuleIndex] );
    }
    else
    {
        foundModuleIndex = index.internalId() & 0x00ffffff;
        if( foundModuleIndex >= static_cast<int>(m_foundModules.size()) ||
            index.row() >= static_cast<int>(m_foundModules[foundModuleIndex].ipAddresses.size()) )   //invalid address index
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
            return QString::fromLatin1("https://%1:%2").arg(moduleData.ipAddresses[moduleAddressIndex]).arg(moduleData.params[QString::fromLatin1("port")]);

        case SeedRole:
            return moduleData.seed;

        case IpRole:
            return moduleData.ipAddresses[moduleAddressIndex];

        case PortRole:
            return moduleData.params[QString::fromLatin1("port")].toInt();

        default:
            return QVariant();
    }
}

bool FoundEnterpriseControllersModel::hasChildren( const QModelIndex& parent ) const
{
    QMutexLocker lk( &m_mutex );

    if( !parent.isValid() )
        return true;

    if( parent.internalId() != 0 ||
        parent.column() > 0 ||
        parent.row() < 0 ||
        parent.row() >= static_cast<int>(m_foundModules.size()) )
    {
        return false;
    }

    return m_foundModules[parent.row()].ipAddresses.size() > 1;
}

QVariant FoundEnterpriseControllersModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if( role != Qt::DisplayRole ||
        orientation != Qt::Horizontal ||
        section != 0 )
    {
        return QVariant();
    }

    return tr("Enterprise Controller addresses");
}

//!Implementation of QAbstractItemModel::index
QModelIndex	FoundEnterpriseControllersModel::index( int row, int column, const QModelIndex& parent ) const
{
    QMutexLocker lk( &m_mutex );

    return indexNonSafe( row, column, parent );
}

QModelIndex	FoundEnterpriseControllersModel::parent( const QModelIndex& index ) const
{
    if( index.column() != 0 || index.internalId() == 0 )
        return QModelIndex();

    const int foundModuleIndex = index.internalId() & 0x00ffffff;
    if( foundModuleIndex >= static_cast<int>(m_foundModules.size()) )
        return QModelIndex();

    return createIndex( foundModuleIndex, 0, (void*)0 );
}

int FoundEnterpriseControllersModel::columnCount( const QModelIndex& index ) const
{
    Q_UNUSED(index)
    return 1;
}

//!Implementation of QAbstractItemModel::rowCount
int	FoundEnterpriseControllersModel::rowCount( const QModelIndex& parent ) const
{
    if( !parent.isValid() )
        return (int) m_foundModules.size();

    if( parent.internalId() != 0 || //element with ip address
        parent.column() != 0 ||
        parent.row() < 0 ||
        parent.row() >= static_cast<int>(m_foundModules.size()) )
    {
        return 0;
    }

    //returning number of IPs of enterprise controller
    return (int) m_foundModules[parent.row()].ipAddresses.size();
}

void FoundEnterpriseControllersModel::remoteModuleFound(
    const QString& moduleID,
    const QString& /*moduleVersion*/,
    const QString& /*systemName*/,
    const TypeSpecificParamMap& moduleParameters,
    const QString& /*localInterfaceAddress*/,
    const QString& remoteHostAddressVal,
    bool isLocal,
    const QString& seed )
{
    QMutexLocker lk( &m_mutex );

    if( moduleID != nxMediaServerId )
        return;
    if( !moduleParameters.contains(QString::fromLatin1("port")) )
        return;

    const QString& remoteHostAddress = isLocal ? QString::fromLatin1("127.0.0.1") : remoteHostAddressVal;

    const QString& url = QString::fromLatin1("https://%1:%2").arg(remoteHostAddress).arg(moduleParameters[QString::fromLatin1("port")]);
    bool isNewElement = false;
    vector<FoundModuleData>::iterator it = std::find_if( m_foundModules.begin(), m_foundModules.end(), IsSeedEqualPred(seed) );
    if( it == m_foundModules.end() )
    {
        FoundModuleData newModuleData;
        newModuleData.url = url;
        newModuleData.seed = seed;
        //searching place to insert new element in order of increase of url
        it = std::lower_bound( m_foundModules.begin(), m_foundModules.end(), newModuleData );
        if( it == m_foundModules.end() || it->url != newModuleData.url )
            it = m_foundModules.insert( it, newModuleData );
        isNewElement = true;
    }

    //if such already exists, updating it's data
    if( std::find( it->ipAddresses.begin(), it->ipAddresses.end(), remoteHostAddress ) == it->ipAddresses.end() )
        it->ipAddresses.push_back( remoteHostAddress );
    it->params = moduleParameters;
    const QModelIndex& updatedElemIndex = indexNonSafe( it - m_foundModules.begin(), 0 );

    lk.unlock();

    if( isNewElement )
    {
        beginResetModel();
        endResetModel();
    }
    else
    {
        emit dataChanged( updatedElemIndex, updatedElemIndex );
    }
}

void FoundEnterpriseControllersModel::remoteModuleLost(
    const QString& moduleID,
    const TypeSpecificParamMap& /*moduleParameters*/,
    const QString& /*remoteHostAddress*/,
    bool isLocal,
    const QString& seed )
{
    Q_UNUSED(isLocal)
    QMutexLocker lk( &m_mutex );

    if( moduleID != nxMediaServerId )
        return;

    vector<FoundModuleData>::iterator it = std::find_if( m_foundModules.begin(), m_foundModules.end(), IsSeedEqualPred(seed) );
    if( it == m_foundModules.end() )
        return;
    m_foundModules.erase( it );

    lk.unlock();

    beginResetModel();
    endResetModel();
}

QString FoundEnterpriseControllersModel::getDisplayStringForEnterpriseControllerRootNode( const FoundModuleData& moduleData ) const
{
    QString port = moduleData.params[lit("port")];

    QString result;
    for(int i = 0; i < moduleData.ipAddresses.size(); ++i) {
        if(i > 0)
            result += lit(", ");
        result += lit("%1:%2").arg(moduleData.ipAddresses[i]).arg(port);
    }
    return result;
}

QString FoundEnterpriseControllersModel::getDisplayStringForEnterpriseControllerAddressNode( const FoundModuleData& /*moduleData*/, const QString& address ) const
{
    return address;
}

QModelIndex	FoundEnterpriseControllersModel::indexNonSafe( int row, int column, const QModelIndex& parent ) const
{
    if( column != 0 )
        return QModelIndex();
    if( !parent.isValid() )
    {
        if( row >= static_cast<int>(m_foundModules.size()) )
            return QModelIndex();
        return createIndex( row, column, (void*)0 );
    }
    else
    {
        if( parent.internalId() != 0 ||
            parent.row() >= static_cast<int>(m_foundModules.size()) ||
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
