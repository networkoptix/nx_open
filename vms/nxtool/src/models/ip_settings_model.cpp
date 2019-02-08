
#include "ip_settings_model.h"

#include <functional>

#include <base/types.h>
#include <helpers/model_change_helper.h>

namespace api = nx::vms::server::api;

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

class rtu::IpSettingsModel::Impl
{
public:
    Impl(bool singleSelection
        , const InterfaceInfoList &interfaces);
    
    virtual ~Impl();
    
    bool isSingleSelection() const;
    
    const InterfaceInfoList &interfaces() const;

    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
    
private:
    const bool m_isSingleSelection;
    const InterfaceInfoList m_addresses;
};


rtu::IpSettingsModel::Impl::Impl(bool singleSelection
    , const InterfaceInfoList &interfaces)
    : m_isSingleSelection(singleSelection)
    , m_addresses(interfaces)
{
}

rtu::IpSettingsModel::Impl::~Impl() {}

bool rtu::IpSettingsModel::Impl::isSingleSelection() const
{
    return m_isSingleSelection;
}

const api::InterfaceInfoList &rtu::IpSettingsModel::Impl::interfaces() const
{
    return m_addresses;
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
    
    const api::InterfaceInfo &info = m_addresses.at(row);
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

rtu::IpSettingsModel::IpSettingsModel(int count
    , const InterfaceInfoList &interfaces
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_helper(CREATE_MODEL_CHANGE_HELPER(this))
    , m_impl(new Impl(count <= kSingleSelection, interfaces))
{
}

rtu::IpSettingsModel::~IpSettingsModel()
{
}

bool rtu::IpSettingsModel::isSingleSelection() const
{
    return m_impl->isSingleSelection();
}

const api::InterfaceInfoList &rtu::IpSettingsModel::interfaces() const
{
    return m_impl->interfaces();
}

void rtu::IpSettingsModel::resetTo(IpSettingsModel *source)
{
    if (!source)
        return;

    const auto resetGuard = m_helper->resetModelGuard();
    Q_UNUSED(resetGuard);

    m_impl.reset(new Impl(source->isSingleSelection(), source->interfaces()));
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



