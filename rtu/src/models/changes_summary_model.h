
#pragma once

#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    class ChangesSummaryModel : public QAbstractListModel
    {
        Q_OBJECT
        
    public:
        explicit ChangesSummaryModel(bool successfulChangesModel
            , QObject *parent = nullptr);
        
        virtual ~ChangesSummaryModel();
        
        Q_PROPERTY(int changesCount READ changesCount NOTIFY changesCountChanged)
        Q_PROPERTY(int columnsCount READ columnsCount NOTIFY columnsCountChanged)
        
    public:
        void addRequestResult(const ServerInfo &info
            , const QString &request
            , const QString &value
            , const QString &errorReason = QString());
        
        int changesCount() const;
        
        int columnsCount() const;
        
    signals:
        void changesCountChanged();
        
        void columnsCountChanged();
        
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
