/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/

#ifndef FOUNDENTERPRISECONTROLLERSMODEL_H
#define FOUNDENTERPRISECONTROLLERSMODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QMutex>

#include <utils/network/networkoptixmodulerevealcommon.h>

struct QnModuleInformation;
class QnModuleFinder;

//!Connects to NetworkOptixModuleFinder and holds found Servers list
class QnFoundEnterpriseControllersModel : public QAbstractItemModel {
    Q_OBJECT
public:
    // TODO: #Elric #ak use global roles from Qn
    enum Role
    {
        UrlRole = Qt::UserRole + 1,
        SeedRole,
        IpRole,
        PortRole
    };

    //!Connects to finder's signals
    QnFoundEnterpriseControllersModel(QnModuleFinder* const finder);

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
    void remoteModuleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress);
    void remoteModuleLost(const QnModuleInformation &moduleInformation);

protected:
    class FoundModuleData {
    public:
        QUuid seed;
        QString url;
        QList<QString> ipAddresses;
        quint16 port;

        bool operator<( const FoundModuleData& right ) const
        {
            return url < right.url; //sorting by url
        }
    };

    virtual QString getDisplayStringForEnterpriseControllerRootNode( const FoundModuleData& moduleData ) const;
    virtual QString getDisplayStringForEnterpriseControllerAddressNode( const FoundModuleData& moduleData, const QString& address ) const;

private:
    QList<FoundModuleData> m_foundModules;
    mutable QMutex m_mutex;

    QModelIndex	indexNonSafe( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
};

#endif  //FOUNDENTERPRISECONTROLLERSMODEL_H
