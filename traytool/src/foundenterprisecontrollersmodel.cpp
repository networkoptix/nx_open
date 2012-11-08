/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#include "foundenterprisecontrollersmodel.h"

#include <algorithm>


using namespace std;

FoundEnterpriseControllersModel::FoundEnterpriseControllersModel( NetworkOptixModuleFinder* const finder )
{
    QObject::connect(
        finder,
        SIGNAL(moduleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, const QString&)),
        this,
        SLOT(remoteModuleFound(const QString&, const QString&, const TypeSpecificParamMap&, const QString&, const QString&, const QString&)) );
    QObject::connect(
        finder,
        SIGNAL(moduleLost(const QString&, const TypeSpecificParamMap&, const QString&, const QString&)),
        this,
        SLOT(remoteModuleLost(const QString&, const TypeSpecificParamMap&, const QString&, const QString&)) );
}

//!Implementation of QAbstractItemModel::data
QVariant FoundEnterpriseControllersModel::data( const QModelIndex& index, int role ) const
{
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
        {
            QString displayString;
            //displayString += tr("Enterprise controller (");
            displayString += tr("port ");
            displayString += QString::fromAscii(" ") + m_foundModules[foundModuleIndex].params["port"];
            displayString += QString::fromAscii(", ip: ");
            for( std::vector<QString>::size_type
                i = 0;
                i < m_foundModules[foundModuleIndex].ipAddresses.size();
                ++i )
            {
                if( i > 0 )
                    displayString += QString::fromAscii(", ");
                displayString += m_foundModules[foundModuleIndex].ipAddresses[i];
            }
            //displayString += QString::fromAscii(")");
            return displayString;
        }
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
            return m_foundModules[foundModuleIndex].ipAddresses[index.row()];
    }
    const FoundModuleData& moduleData = m_foundModules[foundModuleIndex];

    switch( role )
    {
        case urlRole:
            return "https://"+moduleData.ipAddresses[moduleAddressIndex]+":"+moduleData.params["port"];

        case seedRole:
            return moduleData.seed;

        case appServerIPRole:
            return moduleData.ipAddresses[moduleAddressIndex];

        case appServerPortRole:
            return moduleData.params["port"].toInt();

        default:
            return QVariant();
    }
}

bool FoundEnterpriseControllersModel::hasChildren( const QModelIndex& parent ) const
{
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

    return tr("Enterprise controller address");
}

//!Implementation of QAbstractItemModel::index
QModelIndex	FoundEnterpriseControllersModel::index( int row, int column, const QModelIndex& parent ) const
{
    if( column != 0 )
        return QModelIndex();
    if( !parent.isValid() )
    {
        if( row >= static_cast<int>(m_foundModules.size()) )
            return QModelIndex();
        return createIndex( row, column, 0 );
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

QModelIndex	FoundEnterpriseControllersModel::parent( const QModelIndex& index ) const
{
    if( index.column() != 0 || index.internalId() == 0 )
        return QModelIndex();

    const size_t foundModuleIndex = index.internalId() & 0x00ffffff;
    if( foundModuleIndex >= static_cast<int>(m_foundModules.size()) )
        return QModelIndex();

    return createIndex( foundModuleIndex, 0, 0 );
}

//!Implementation of QAbstractItemModel::rowCount
int	FoundEnterpriseControllersModel::rowCount( const QModelIndex& parent ) const
{
    if( !parent.isValid() )
        return m_foundModules.size();

    if( parent.internalId() != 0 || //element with ip address
        parent.column() != 0 ||
        parent.row() < 0 ||
        parent.row() >= static_cast<int>(m_foundModules.size()) )
    {
        return 0;
    }

    //returning number of IPs of enterprise controller
    return m_foundModules[parent.row()].ipAddresses.size();
}

void FoundEnterpriseControllersModel::remoteModuleFound(
    const QString& moduleID,
    const QString& /*moduleVersion*/,
    const TypeSpecificParamMap& moduleParameters,
    const QString& /*localInterfaceAddress*/,
    const QString& remoteHostAddress,
    const QString& seed )
{
    if( moduleID != NX_ENTERPISE_CONTROLLER_ID )
        return;
    if( !moduleParameters.contains("port") )
        return;

    const QString& url = "https://"+remoteHostAddress+":"+moduleParameters["port"];
    bool isNewElement = false;
    vector<FoundModuleData>::iterator it = std::find_if( m_foundModules.begin(), m_foundModules.end(), IsSeedEqualPred(seed) );
    if( it == m_foundModules.end() )
    {
        FoundModuleData newModuleData;
        newModuleData.url = url;
        newModuleData.seed = seed;
        beginResetModel();
        //searching place to insert new element in order of increase of url
        it = std::lower_bound( m_foundModules.begin(), m_foundModules.end(), newModuleData );
        if( it == m_foundModules.end() || it->url != newModuleData.url )
            it = m_foundModules.insert( it, newModuleData );
        isNewElement = true;
    }

    //if such already exists, updating it's data
    it->url = url;
    it->ipAddresses.push_back( remoteHostAddress );
    it->params = moduleParameters;
    if( isNewElement )
    {
        endResetModel();
    }
    else
    {
        const QModelIndex& updatedElemIndex = index(it - m_foundModules.begin(), 0);
        emit dataChanged( updatedElemIndex, updatedElemIndex );
    }
}

void FoundEnterpriseControllersModel::remoteModuleLost(
    const QString& moduleID,
    const TypeSpecificParamMap& /*moduleParameters*/,
    const QString& /*remoteHostAddress*/,
    const QString& seed )
{
    if( moduleID != NX_ENTERPISE_CONTROLLER_ID )
        return;

    vector<FoundModuleData>::iterator it = std::find_if( m_foundModules.begin(), m_foundModules.end(), IsSeedEqualPred(seed) );
    if( it == m_foundModules.end() )
        return;
    beginResetModel();
    m_foundModules.erase( it );
    endResetModel();
}
