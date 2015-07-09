
#pragma once

#include <QStringListModel>

#include <base/types.h>

namespace rtu
{
    class TimeZonesModel : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(int initIndex READ initIndex NOTIFY initIndexChanged)
        Q_PROPERTY(int currentTimeZoneIndex READ currentTimeZoneIndex NOTIFY currentTimeZoneIndexChanged)

    public:
        explicit TimeZonesModel(const ServerInfoList &selectedServers
            , QObject *parent = nullptr);
        
        virtual ~TimeZonesModel();
        
    public:
        int initIndex() const;       
        int currentTimeZoneIndex();

    public slots:
        bool isValidValue(int index);
        QByteArray timeZoneIdByIndex(int index) const;

    signals:
        void initIndexChanged();

        void currentTimeZoneIndexChanged();
        
    private:
        virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

        virtual QVariant data(const QModelIndex &index , int role = Qt::DisplayRole) const override;

    private:
        class Impl;
        Impl * const m_impl;
    };
}
