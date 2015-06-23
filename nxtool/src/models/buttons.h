
#pragma once

#include <QAbstractListModel>

#include <base/types.h>

namespace rtu
{
    class Buttons : public QAbstractListModel
    {
        Q_OBJECT
        
        Q_PROPERTY(int buttons READ buttons WRITE setButtons NOTIFY buttonsChanged)
        
        
    public:
        enum ButtonType
        {
            Yes                 = 0x01
            , ApplyChanges      = 0x02
            , Ok                = 0x04
            , No                = 0x08
            , DiscardChanges    = 0x10
            , Cancel            = 0x20
            
        };
        Q_ENUMS(ButtonType)
        
        Buttons(QObject *parent = nullptr);
        
        virtual ~Buttons();
        
    public:
        int buttons() const;
        
        void setButtons(int buttons);
        
    signals:
        void buttonsChanged();
        
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
