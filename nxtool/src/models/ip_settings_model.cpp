
#include "ip_settings_model.h"

#include <functional>

#include <base/types.h>
#include <base/server_info.h>
#include <models/servers_selection_model.h>

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
    
    typedef std::function<void (int first, int last)> IndexedDataActionFunc;
    typedef std::function<void ()> DataActionFunc;
    
    enum { kSingleSelection = 1 };
    rtu::InterfaceInfoList extractFromSelectionModel(rtu::ServersSelectionModel *model)
    {
        const rtu::ServerInfoPtrContainer &servers = model->selectedServers();
        
        if (servers.empty())
            return rtu::InterfaceInfoList();
        
        const rtu::ServerInfo &firstServerInfo = **servers.begin();
        if (!firstServerInfo.hasExtraInfo())
            return rtu::InterfaceInfoList();
        
        if (servers.size() == kSingleSelection)
            return (firstServerInfo.extraInfo().interfaces);
    
        const rtu::InterfaceInfoList &interfaces = firstServerInfo.extraInfo().interfaces;
        if (interfaces.empty())
            return rtu::InterfaceInfoList();
        
        const Qt::CheckState initialUseDHCP = interfaces.begin()->useDHCP;
        
        const bool diffUseDHCP = (servers.end() == std::find_if(servers.begin(), servers.end()
            , [initialUseDHCP](const rtu::ServerInfo *info)
        {
            const rtu::InterfaceInfoList &addresses = info->extraInfo().interfaces;
            return (addresses.end() == std::find_if(addresses.begin(), addresses.end()
                , [initialUseDHCP](const rtu::InterfaceInfo &addr)
                { return (addr.useDHCP != initialUseDHCP); }));
        }));
    
        const Qt::CheckState useDHCP = (diffUseDHCP ? Qt::PartiallyChecked : initialUseDHCP);
        rtu::InterfaceInfoList addresses;
        addresses.push_back(rtu::InterfaceInfo(useDHCP));
        return addresses;
    }
}

///

class rtu::IpSettingsModel::Impl : public QObject
{
public:
    Impl(ServersSelectionModel *selectionModel
        , QObject *parent
        , const IndexedDataActionFunc &dataChanged
        , const IndexedDataActionFunc &beginInsertRow
        , const DataActionFunc &endInsertRow
        , const DataActionFunc &beginResetModel
        , const DataActionFunc &endResetModel);
    
    virtual ~Impl();
    
    bool isSingleSelection() const;
    
    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
    
private:
    const bool m_isSingleSelection;
    const rtu::InterfaceInfoList m_addresses;
    
    const IndexedDataActionFunc m_dataChanged;
    const IndexedDataActionFunc m_beginInsertRow;
    const DataActionFunc m_endInsertRow;
    const DataActionFunc m_beginResetModel;
    const DataActionFunc m_endResetModel;
};


rtu::IpSettingsModel::Impl::Impl(ServersSelectionModel *selectionModel
    , QObject *parent
    , const IndexedDataActionFunc &dataChanged
    , const IndexedDataActionFunc &beginInsertRow
    , const DataActionFunc &endInsertRow
    , const DataActionFunc &beginResetModel
    , const DataActionFunc &endResetModel)

    : QObject(parent)
    , m_isSingleSelection(selectionModel->selectedCount() <= kSingleSelection)
    , m_addresses(extractFromSelectionModel(selectionModel))
    
    , m_dataChanged(dataChanged)
    , m_beginInsertRow(beginInsertRow)
    , m_endInsertRow(endInsertRow)
    , m_beginResetModel(beginResetModel)
    , m_endResetModel(endResetModel)    
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

rtu::IpSettingsModel::IpSettingsModel(ServersSelectionModel *selectionModel
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(selectionModel, this
        , [this](int first, int last){ emit dataChanged(index(first), index(last)); }
        , [this](int first, int last){ emit beginInsertRows(QModelIndex(), first, last); }
        , [this](){ emit endInsertRows(); }
        , [this](){ emit beginResetModel();}
        , [this](){ emit endResetModel();}
        ))
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



