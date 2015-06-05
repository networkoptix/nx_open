
#pragma once

#include <QStringListModel>

#include <models/types.h>

namespace rtu
{
    class TimeZonesModel : public QStringListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(int initIndex READ initIndex NOTIFY initIndexChanged)
       
    public:
        explicit TimeZonesModel(const ServerInfoList &selectedServers
            , QObject *parent = nullptr);
        
        virtual ~TimeZonesModel();
        
    public:
        int initIndex() const;
        
    public slots:
        int onCurrentIndexChanged(int index);
        
    signals:
        void initIndexChanged();
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}
