
#pragma once

#include <QStringListModel>

#include <base/types.h>

namespace rtu
{
    class TimeZonesModel : public QStringListModel
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
        void onCurrentIndexChanged(int index);
        
    signals:
        void initIndexChanged();

        void currentTimeZoneIndexChanged();
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
