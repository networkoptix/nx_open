/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef FOUNDENTERPRISECONTROLLERSMODEL_H
#define FOUNDENTERPRISECONTROLLERSMODEL_H

#include <vector>

#include <QtCore/QAbstractListModel>
#include <QtCore/QMutex>

#include <utils/network/networkoptixmodulefinder.h>


//!Connects to NetworkOptixModuleFinder and holds found enterprise controllers list
class FoundEnterpriseControllersModel
:
    public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role
    {
        UrlRole = Qt::UserRole + 1,
        SeedRole,
        IpRole,
        PortRole
    };

    //!Connects to finder's signals
    FoundEnterpriseControllersModel( NetworkOptixModuleFinder* const finder );

    //!Implementation of QAbstractItemModel::data
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
    //!Implementation of QAbstractItemModel::hasChildren
    virtual bool hasChildren( const QModelIndex& parent = QModelIndex() ) const;
    //!Implementation of QAbstractItemModel::headerData
    virtual QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
    //!Implementation of QAbstractItemModel::index
    virtual QModelIndex	index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
    //!Implementation of QAbstractItemModel::parent
    virtual QModelIndex	parent( const QModelIndex& index ) const;
    //!Implementation of QAbstractItemModel::columnCount
    virtual int columnCount( const QModelIndex& index ) const;
    //!Implementation of QAbstractItemModel::rowCount
    virtual int	rowCount( const QModelIndex& parent = QModelIndex() ) const;

public slots:
    void remoteModuleFound(
        const QString& moduleID,
        const QString& moduleVersion,
        const QString& systemName,
        const TypeSpecificParamMap& moduleParameters,
        const QString& localInterfaceAddress,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& seed );
    void remoteModuleLost(
        const QString& moduleID,
        const TypeSpecificParamMap& moduleParameters,
        const QString& remoteHostAddress,
        bool isLocal,
        const QString& seed );

protected:
    class FoundModuleData {
    public:
        QString seed;
        QString url;
        QList<QString> ipAddresses;
        TypeSpecificParamMap params;

        bool operator<( const FoundModuleData& right ) const
        {
            return url < right.url; //sorting by url
        }
    };

    virtual QString getDisplayStringForEnterpriseControllerRootNode( const FoundModuleData& moduleData ) const;
    virtual QString getDisplayStringForEnterpriseControllerAddressNode( const FoundModuleData& moduleData, const QString& address ) const;

private:
    class IsSeedEqualPred
    {
    public:
        IsSeedEqualPred( const QString& seed )
        :
            m_seed( seed )
        {
        }

        bool operator()( const FoundEnterpriseControllersModel::FoundModuleData& elem ) const
        {
            return elem.seed == m_seed;
        }

    private:
        QString m_seed;
    };

    std::vector<FoundModuleData> m_foundModules;
    mutable QMutex m_mutex;

    QModelIndex	indexNonSafe( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
};

#endif  //FOUNDENTERPRISECONTROLLERSMODEL_H
