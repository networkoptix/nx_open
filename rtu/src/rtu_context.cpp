
#include "rtu_context.h"

#include <change_request_manager.h>
#include <models/time_zones_model.h>
#include <models/ip_settings_model.h>
#include <models/servers_selection_model.h>

#include <helpers/http_client.h>

class rtu::RtuContext::Impl : public QObject
{
public:
    Impl(RtuContext *parent);

    virtual ~Impl();
    
    rtu::ServersSelectionModel *selectionModel();
    
    QAbstractListModel *ipSettingsModel();
    
    rtu::TimeZonesModel *timeZonesModel(QObject *parent);
    
    rtu::ChangeRequestManager *changesManager();
    
    rtu::HttpClient *httpClient();
    
    ///
    
    int selectedServersCount() const;
    
    int selectionPort() const;
   
    QString selectionSystemName() const;
    
    QString selectionPassword() const;
    
    QDateTime selectionDateTime() const;
    
    bool allowEditIpAddresses() const;
    
    void setCurrentPage(int pageId);
    
    int currentPage() const;
    
private:
    rtu::RtuContext * const m_context;
    rtu::HttpClient * const m_httpClient;
    
    rtu::ServersSelectionModel * const m_selectionModel;
    
    rtu::ChangeRequestManager * const m_changesManager;
    
    int m_currentPageIndex;
};

rtu::RtuContext::Impl::Impl(RtuContext *parent)
    : QObject(parent)
    , m_context(parent)
    , m_httpClient(new HttpClient(this))
    
    , m_selectionModel(new ServersSelectionModel(this))
    
    , m_changesManager(new ChangeRequestManager(parent, m_selectionModel, this))

    , m_currentPageIndex(0)    
{
    QObject::connect(m_selectionModel, &ServersSelectionModel::selectionChanged
        , m_context, &RtuContext::selectionChagned);
}

rtu::RtuContext::Impl::~Impl()
{   
}


rtu::ServersSelectionModel *rtu::RtuContext::Impl::selectionModel()
{
    return m_selectionModel;
}

QAbstractListModel *rtu::RtuContext::Impl::ipSettingsModel()
{
    return new IpSettingsModel(m_selectionModel);
}

rtu::TimeZonesModel *rtu::RtuContext::Impl::timeZonesModel(QObject *parent)
{
    return new TimeZonesModel(m_selectionModel->selectedServers(), parent);
}

rtu::ChangeRequestManager *rtu::RtuContext::Impl::changesManager()
{
    return m_changesManager;
}

rtu::HttpClient *rtu::RtuContext::Impl::httpClient()
{
    return m_httpClient;
}


int rtu::RtuContext::Impl::selectedServersCount() const
{
    return m_selectionModel->selectedCount();
}

#include <functional>

template<typename ParameterType
    , typename GetterFunc>
ParameterType getSelectionValue(const rtu::ServerInfoList &servers
    , const ParameterType &emptyValue
    , const ParameterType &differentValue
    , const GetterFunc &getter)
{
    if (servers.empty())
        return emptyValue;
    
    const rtu::ServerInfo * const firstInfo = *servers.begin();
    const ParameterType &value = getter(*firstInfo);
    const auto itDifferentValue = std::find_if(servers.begin(), servers.end()
        , [&value, &getter](const rtu::ServerInfo *info) { return (getter(*info) != value);});
    return (itDifferentValue == servers.end() ? value : differentValue);
}

int rtu::RtuContext::Impl::selectionPort() const
{
    const ServerInfoList selectedServers = m_selectionModel->selectedServers();
    enum { kDefaultPort = 0 };
    
    return getSelectionValue<int>(selectedServers, kDefaultPort, kDefaultPort,
        [](const ServerInfo &info) { return info.baseInfo().port; });
}

QDateTime rtu::RtuContext::Impl::selectionDateTime() const
{
    return QDateTime::currentDateTime();
}

QString rtu::RtuContext::Impl::selectionSystemName() const
{
    const ServerInfoList &selectedServers = m_selectionModel->selectedServers();
    if (selectedServers.empty())
        return QString();
    
    const ServerInfo &info = **selectedServers.begin();
    return (info.baseInfo().flags & ServerFlag::kIsFactory ? QString() : info.baseInfo().systemName);
}

QString rtu::RtuContext::Impl::selectionPassword() const
{
    static const QString kDifferentPasswords = tr("<Different passwords>");
    return getSelectionValue(m_selectionModel->selectedServers(), QString(), kDifferentPasswords
        , [](const ServerInfo &info) -> QString { return info.extraInfo().password; });
}

bool rtu::RtuContext::Impl::allowEditIpAddresses() const
{
    const ServerInfoList selectedServers = m_selectionModel->selectedServers();
    if (selectedServers.size() == 1)
        return true;
    
    enum 
    {
        kEmptyIps = 0
        , kDifferentIpsCount = -1 
        , kSingleIp
    };
    
    const int ipsCount = getSelectionValue<int>(selectedServers, kEmptyIps, kDifferentIpsCount
        , [](const ServerInfo &info) -> int { return info.extraInfo().interfaces.size(); });
    
    return (ipsCount == kSingleIp ? true : false);
}

void rtu::RtuContext::Impl::setCurrentPage(int pageId)
{
    if (m_currentPageIndex == pageId)
        return;
    
    m_currentPageIndex = pageId;
    emit m_context->currentPageChanged();
}

int rtu::RtuContext::Impl::currentPage() const
{
    return m_currentPageIndex;
}

///

rtu::RtuContext::RtuContext(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
}

rtu::RtuContext::~RtuContext()
{
}

QObject *rtu::RtuContext::selectionModel()
{
    return m_impl->selectionModel();
}

QObject *rtu::RtuContext::ipSettingsModel()
{
    return m_impl->ipSettingsModel();
}

#include <QSharedPointer>

QObject *rtu::RtuContext::timeZonesModel(QObject *parent)
{
    return m_impl->timeZonesModel(parent);
}

QObject *rtu::RtuContext::changesManager()
{
    return m_impl->changesManager();
}

rtu::HttpClient *rtu::RtuContext::httpClient()
{
    return m_impl->httpClient();
}

QDateTime rtu::RtuContext::applyTimeZone(const QDate &date
    , const QTime &time
    , const QByteArray &prevTimeZoneId
    , const QByteArray &newTimeZoneId)
{
    qDebug() << prevTimeZoneId << " : " << newTimeZoneId;
    
    const QTimeZone prevTimeZone = (prevTimeZoneId.isEmpty() ? QTimeZone() : QTimeZone(prevTimeZoneId));
    const QTimeZone newTimeZone(newTimeZoneId);
    const QDateTime result = (!prevTimeZone.isValid() ? QDateTime(date, time, newTimeZone)
        : QDateTime(date, time, prevTimeZone).toTimeZone(newTimeZone));
    return QDateTime(result.date(), result.time());
}


int rtu::RtuContext::selectedServersCount() const
{
    return m_impl->selectedServersCount();
}

int rtu::RtuContext::selectionPort() const
{
    return m_impl->selectionPort();
}

QDateTime rtu::RtuContext::selectionDateTime() const
{
    return m_impl->selectionDateTime();
}

QString rtu::RtuContext::selectionSystemName() const
{
    return m_impl->selectionSystemName();
}

QString rtu::RtuContext::selectionPassword() const
{
    return m_impl->selectionPassword();
}

bool rtu::RtuContext::allowEditIpAddresses() const
{
    return m_impl->allowEditIpAddresses();
}

void rtu::RtuContext::tryLoginWith(const QString &password)
{
    return m_impl->selectionModel()->tryLoginWith(password);
}

void rtu::RtuContext::setCurrentPage(int pageId)
{
    m_impl->setCurrentPage(pageId);
}

int rtu::RtuContext::currentPage() const
{
    return m_impl->currentPage();
}


