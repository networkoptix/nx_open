
#pragma once

#include <QStringListModel>

#include <nx/vms/server/api/server_info.h>

namespace rtu
{
    class ModelChangeHelper;

    class TimeZonesModel : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(int initIndex READ initIndex NOTIFY initIndexChanged)
        Q_PROPERTY(int currentTimeZoneIndex READ currentTimeZoneIndex NOTIFY currentTimeZoneIndexChanged)

    public:
        explicit TimeZonesModel(
            const nx::vms::server::api::ServerInfoPtrContainer &selectedServers
            , QObject *parent = nullptr);
        
        virtual ~TimeZonesModel();
        
    public:
        int initIndex() const;       

        int currentTimeZoneIndex() const;

        virtual QVariant data(const QModelIndex &index , int role = Qt::DisplayRole) const override;

        void resetTo(TimeZonesModel *source);

    public slots:
        bool isValidValue(int index);
        QByteArray timeZoneIdByIndex(int index) const;

    signals:
        void initIndexChanged();

        void currentTimeZoneIndexChanged();
        
    private:
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;
        typedef std::unique_ptr<ModelChangeHelper> HelperPtr;

        const HelperPtr m_helper;
        ImplPtr m_impl;
    };
}
