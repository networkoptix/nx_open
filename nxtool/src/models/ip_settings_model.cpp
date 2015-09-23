
#include "ip_settings_model.h"

#include <functional>

#include <base/types.h>
#include <base/selection.h>
#include <base/server_info.h>

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
        
        , kAdapterNameRoleId
        , kIpAddressRoleId
        , kSubnetMaskRoleId
        , kIsDHCPRoleId
        , kReadableNameRoleId
        , kDnsRoleId
        , kGatewayRoleId
        
        , kLastCustomRoleId
    };
    
    enum { kSingleSelection = 1 };

    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles result;
            
        result.insert(kAdapterNameRoleId, "adapterName");
        result.insert(kIpAddressRoleId, "address");
        result.insert(kSubnetMaskRoleId, "subnetMask");
        result.insert(kIsDHCPRoleId, "useDHCP");
        result.insert(kDnsRoleId, "dns");
        result.insert(kGatewayRoleId, "gateway");
        result.insert(kReadableNameRoleId, "readableName");
        return result;
    }();
}

///

class rtu::IpSettingsModel::Impl : public QObject
{
public:
    Impl(Selection *selection
        , QObject *parent);
    
    virtual ~Impl();
    
    bool isSingleSelection() const;
    
    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
    
private:
    const bool m_isSingleSelection;
    const InterfaceInfoList m_addresses;
};


rtu::IpSettingsModel::Impl::Impl(Selection *selection
    , QObject *parent)

    : QObject(parent)
    , m_isSingleSelection(selection ? selection->count() <= kSingleSelection : 0)
    , m_addresses(selection ? selection->aggregatedInterfaces() : InterfaceInfoList())
{
}

rtu::IpSettingsModel::Impl::~Impl() {}

bool rtu::IpSettingsModel::Impl::isSingleSelection() const
{
    return m_isSingleSelection;
}

int rtu::IpSettingsModel::Impl::rowCount() const
{
    return m_addresses.size();
}

QVariant rtu::IpSettingsModel::Impl::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();
    if ((row < 0) || (row > rowCount())
        || (role < kFirstCustomRoleId) || (role >= kLastCustomRoleId))
    {
        return QVariant();
    }
    
    const InterfaceInfo &info = m_addresses.at(row);
    switch(role)
    {
    case kAdapterNameRoleId:
        return info.name;
    case kIpAddressRoleId:
        return info.ip;
    case kSubnetMaskRoleId:
        return info.mask;
    case kIsDHCPRoleId:
        return info.useDHCP;
    case kReadableNameRoleId:
        return QString("Interface #%1").arg(QString::number(row + 1));
    case kDnsRoleId:
        return info.dns;
    case kGatewayRoleId:
        return info.gateway;
    default:
        return QVariant();
    }
}

///

rtu::IpSettingsModel::IpSettingsModel(Selection *selection
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(selection, this))
{
}

rtu::IpSettingsModel::~IpSettingsModel()
{
}

bool rtu::IpSettingsModel::isSingleSelection() const
{
    return m_impl->isSingleSelection();
}

int rtu::IpSettingsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_impl->rowCount();
}

QVariant rtu::IpSettingsModel::data(const QModelIndex &index
    , int role) const
{
    return m_impl->data(index, role);
}

rtu::Roles rtu::IpSettingsModel::roleNames() const
{
    return kRoles;
}



