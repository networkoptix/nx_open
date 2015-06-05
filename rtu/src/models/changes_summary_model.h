
#pragma once

#include <QAbstractListModel>

#include <models/types.h>

namespace rtu
{
    class ChangesSummaryModel : public QAbstractListModel
    {
    public:
        explicit ChangesSummaryModel(bool successfulChangesModel
            , QObject *parent = nullptr);
        
        virtual ~ChangesSummaryModel();
        
    public:
        void addRequestResult(const ServerInfo &info
            , const QString &request
            , const QString &value
            , const QString &errorReason = QString());
        
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
