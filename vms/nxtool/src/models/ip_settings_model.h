
#pragma once

#include <QtQml>
#include <QAbstractListModel>

#include <base/types.h>
#include <nx/vms/server/api/server_info.h>

namespace rtu
{
    class Selection;
    class ModelChangeHelper;

    class IpSettingsModel : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(bool isSingleSelection READ isSingleSelection NOTIFY isSingleSelectionChanged)
        
    public:
        typedef nx::vms::server::api::InterfaceInfoList InterfaceInfoList;

        IpSettingsModel(int count
            , const InterfaceInfoList &interfaces
            , QObject *parent = nullptr);
        
        virtual ~IpSettingsModel();
        
    public:
        bool isSingleSelection() const;
        
        const InterfaceInfoList &interfaces() const;

        void resetTo(IpSettingsModel *source);

    signals:
        void isSingleSelectionChanged();
        
    private:
        int rowCount(const QModelIndex &parent = QModelIndex()) const;
        
        QVariant data(const QModelIndex &index
            , int role = Qt::DisplayRole) const;
        
        Roles roleNames() const;
        
    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;
        typedef std::unique_ptr<ModelChangeHelper> ModelChangeHelperPtr;

        const ModelChangeHelperPtr m_helper;
        ImplPtr m_impl;
    };
}
