
#include "rtu_context.h"

#include <base/selection.h>
#include <base/servers_finder.h>
#include <base/changes_manager.h>
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
    
    rtu::ChangesManager *changesManager();
    
    rtu::HttpClient *httpClient();

    ///
    
    void setCurrentPage(rtu::Constants::Pages pageId);
    
    rtu::Constants::Pages currentPage() const;
    
    Selection *selection() const;

private:
    typedef QScopedPointer<Selection> SelectionPointer;

    rtu::RtuContext * const m_owner;
    rtu::HttpClient * const m_httpClient;
    rtu::ServersSelectionModel * const m_selectionModel;
    
    SelectionPointer m_selection;
    rtu::ChangesManager * const m_changesManager;
    const rtu::ServersFinder::Holder m_serversFinder;
    
    rtu::Constants::Pages m_currentPageIndex;
};

rtu::RtuContext::Impl::Impl(RtuContext *parent)
    : QObject(parent)
    , m_owner(parent)
    , m_httpClient(new HttpClient(this))
    , m_selectionModel(new ServersSelectionModel(this))

    , m_selection()
    , m_changesManager(new ChangesManager(
        parent, m_httpClient, m_selectionModel, this))
    , m_serversFinder(ServersFinder::create())

    , m_currentPageIndex(rtu::Constants::SettingsPage)
{
    QObject::connect(m_selectionModel, &ServersSelectionModel::selectionChanged
        , [this]()
    {
        m_selection.reset(new Selection(m_selectionModel));
        emit m_owner->selectionChanged();
    });

    QObject::connect(m_serversFinder.data(), &ServersFinder::serverAdded
        , m_selectionModel, &ServersSelectionModel::addServer);
    QObject::connect(m_serversFinder.data(), &ServersFinder::serverChanged
        , m_selectionModel, &ServersSelectionModel::changeServer);
    QObject::connect(m_serversFinder.data(), &ServersFinder::serversRemoved
        , m_selectionModel, &ServersSelectionModel::removeServers);

    QObject::connect(m_serversFinder.data(), &ServersFinder::serverDiscovered
        , m_changesManager, &ChangesManager::serverDiscovered);
    QObject::connect(m_serversFinder.data(), &ServersFinder::serverDiscovered
        , m_selectionModel, &ServersSelectionModel::serverDiscovered);
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

rtu::ChangesManager *rtu::RtuContext::Impl::changesManager()
{
    return m_changesManager;
}

void rtu::RtuContext::Impl::setCurrentPage(rtu::Constants::Pages pageId)
{
    if (m_currentPageIndex == pageId)
        return;
    
    m_currentPageIndex = pageId;
    emit m_owner->currentPageChanged();
}

rtu::Constants::Pages rtu::RtuContext::Impl::currentPage() const
{
    return m_currentPageIndex;
}

rtu::Selection *rtu::RtuContext::Impl::selection() const
{
    return m_selection.data();
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

QObject *rtu::RtuContext::timeZonesModel(QObject *parent)
{
    return m_impl->timeZonesModel(parent);
}

QObject *rtu::RtuContext::changesManager()
{
    return m_impl->changesManager();
}

#include <helpers/time_helper.h>

QDateTime rtu::RtuContext::applyTimeZone(const QDate &date
    , const QTime &time
    , const QByteArray &prevTimeZoneId
    , const QByteArray &newTimeZoneId)
{
    if (prevTimeZoneId.isEmpty() || newTimeZoneId.isEmpty())
        return QDateTime();

    const QTimeZone prevTimeZone(prevTimeZoneId);
    const QTimeZone nextTimeZone(newTimeZoneId);
    if (date.isNull() || time.isNull() || !prevTimeZone.isValid()
        || !nextTimeZone.isValid() || !date.isValid() || !time.isValid())
    {
        return QDateTime();
    }
    
    return convertDateTime(date, time, prevTimeZone, nextTimeZone).toLocalTime();
}

void rtu::RtuContext::tryLoginWith(const QString &primarySystem
    , const QString &password)
{
    return m_impl->selectionModel()->tryLoginWith(primarySystem, password
        , [this, primarySystem]() 
    {
        emit this->loginOperationFailed(primarySystem);
    });
}

void rtu::RtuContext::setCurrentPage(int pageId)
{
    m_impl->setCurrentPage(static_cast<Constants::Pages>(pageId));
}

int rtu::RtuContext::currentPage() const
{
    return m_impl->currentPage();
}

QObject *rtu::RtuContext::selection() const
{
    return m_impl->selection();
}

