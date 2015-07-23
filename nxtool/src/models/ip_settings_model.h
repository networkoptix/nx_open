
#pragma once

#include <QtQml>
#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    class ServersSelectionModel;
    
    class IpSettingsModel : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(bool isSingleSelection READ isSingleSelection NOTIFY isSingleSelectionChanged)
        
    public:
        IpSettingsModel(ServersSelectionModel *selectionModel
            , QObject *parent = nullptr);
        
        virtual ~IpSettingsModel();
        
    public:
        bool isSingleSelection() const;
        
    signals:
        void isSingleSelectionChanged();
        
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
