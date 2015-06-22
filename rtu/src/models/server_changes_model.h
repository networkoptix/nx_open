
#pragma once

#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    class ServerChangesModel : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(int columnsCount READ columnsCount NOTIFY columnsCountChanged)
        Q_PROPERTY(int changesCount READ changesCount NOTIFY changesCountChanged)
    public:
        explicit ServerChangesModel(bool successfulRequstsModel
            , QObject *parent = nullptr);
        
        virtual ~ServerChangesModel();
        
        enum DataType
        {
            kRequestCaption
            , kRequestValue
            , kRequestSuccessful
            , kRequestErrorReason
        };
        
        Q_ENUMS(DataTypes)

    public:
        void addRequestResult(const QString &request
            , const QString &value
            , const QString &errorReason);
        
        int columnsCount() const;
        
        int changesCount() const;
        
    signals:
        void columnsCountChanged();
        
        void changesCountChanged();
        
    private:
        int rowCount(const QModelIndex &parent = QModelIndex()) const;
        
        QVariant data(const QModelIndex &index
            , int role = Qt::DisplayRole) const;
        
        Roles roleNames() const;
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
