
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
        
    public:
        void addRequestResult(const ServerInfo &info
            , const QString &request
            , const QString &value
            , const QString &errorReason = QString());
        
        int changesCount() const;
        
    signals:
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
