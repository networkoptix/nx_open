/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef FOUNDENTERPRISECONTROLLERSMODEL_H
#define FOUNDENTERPRISECONTROLLERSMODEL_H

#include <vector>

#include <QAbstractListModel>

#include <utils/network/networkoptixmodulefinder.h>


//!Connects to NetworkOptixModuleFinder and holds found enterprise controllers list
class FoundEnterpriseControllersModel
:
    public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        urlRole = Qt::DisplayRole,
        seedRole = Qt::UserRole + 1,
        appServerIPRole,
        appServerPortRole
    };

    //!Connects to finder's signals
    FoundEnterpriseControllersModel( NetworkOptixModuleFinder* const finder );

    //!Implementation of QAbstractItemModel::data
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
    //!Implementation of QAbstractItemModel::index
    virtual QModelIndex	index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
    //!Implementation of QAbstractItemModel::rowCount
    virtual int	rowCount( const QModelIndex& parent = QModelIndex() ) const;

public slots:
    void remoteModuleFound(
        const QString& moduleID,
        const QString& moduleVersion,
        const TypeSpecificParamMap& moduleParameters,
        const QString& localInterfaceAddress,
        const QString& remoteHostAddress,
        const QString& seed );
    void remoteModuleLost(
        const QString& moduleID,
        const TypeSpecificParamMap& moduleParameters,
        const QString& remoteHostAddress,
        const QString& seed );

private:
    class FoundModuleData
    {
    public:
        QString seed;
        QString url;
        QString ipAddress;
        TypeSpecificParamMap params;
    };

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
};

#endif  //FOUNDENTERPRISECONTROLLERSMODEL_H
