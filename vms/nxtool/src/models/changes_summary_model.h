
#pragma once

#include <QAbstractListModel>

#include <nx/vms/server/api/server_info.h>
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
        Q_PROPERTY(bool isSuccessChangesModel READ isSuccessChangesModel NOTIFY isSuccessChangesModelChanged)
        
    public:
        typedef nx::vms::server::api::ServerInfo ServerInfo;

        void addRequestResult(const ServerInfo &info
            , const QString &request
            , const QString &value
            , const QString &errorReason = QString());
        
        int changesCount() const;
        
        bool isSuccessChangesModel() const;
        
    signals:
        void changesCountChanged();
        
        void isSuccessChangesModelChanged();
        
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
