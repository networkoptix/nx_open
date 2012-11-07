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
    if( index.column() > 0 || index.row() >= static_cast<int>(m_foundModules.size()) )
        return QVariant();

    switch( role )
    {
        case urlRole:   //same as Qt::DisplayRole
            return m_foundModules[index.row()].url;

        case seedRole:
            return m_foundModules[index.row()].seed;

        case appServerIPRole:
            return m_foundModules[index.row()].ipAddress;

        case appServerPortRole:
            return m_foundModules[index.row()].params["port"].toInt();

        default:
            return QVariant();
    }
}

//!Implementation of QAbstractItemModel::index
QModelIndex	FoundEnterpriseControllersModel::index( int row, int column, const QModelIndex& parent ) const
{
    if( parent.isValid() || column > 0 || row >= static_cast<int>(m_foundModules.size()) )
        return QModelIndex();

    return createIndex( row, column, NULL );
}

//!Implementation of QAbstractItemModel::rowCount
int	FoundEnterpriseControllersModel::rowCount( const QModelIndex& parent ) const
{
    if( parent.isValid() )
        return 0;

    return m_foundModules.size();
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
    it->ipAddress = remoteHostAddress;
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
