
#include "rtu_context.h"

#include <version.h>

#include <base/selection.h>
#include <base/servers_finder.h>
#include <base/changes_manager.h>
#include <base/apply_changes_task.h>
#include <models/time_zones_model.h>
#include <models/ip_settings_model.h>
#include <models/changes_progress_model.h>
#include <models/servers_selection_model.h>

#include <helpers/time_helper.h>
#include <helpers/rest_client.h>

class rtu::RtuContext::Impl : public QObject
{
public:
    Impl(RtuContext *parent);

    virtual ~Impl();
    
    rtu::ServersSelectionModel *selectionModel();
    
    QAbstractListModel *ipSettingsModel();
    
    rtu::TimeZonesModel *timeZonesModel(QObject *parent);
    
    rtu::ChangesManager *changesManager();
    
    ApplyChangesTask *progressTask();

    void showProgressTask(const ApplyChangesTaskPtr &task);

    void applyTaskCompleted(const ApplyChangesTaskPtr &task);

    void showProgressTaskFromList(int index);

    void removeProgressTask(int index);

    ///
    
    void setCurrentPage(rtu::Constants::Pages pageId);
    
    bool showWarnings() const;

    void setShowWarnings(bool show);

    rtu::Constants::Pages currentPage() const;
    
    Selection *selection() const;

private:
    typedef QScopedPointer<Selection> SelectionPointer;

    rtu::RtuContext * const m_owner;
    rtu::ServersSelectionModel * const m_selectionModel;
    
    SelectionPointer m_selection;
    const rtu::ServersFinder::Holder m_serversFinder;
    rtu::ChangesManager * const m_changesManager;
    ApplyChangesTaskPtr m_progressTask;
    rtu::Constants::Pages m_currentPageIndex;
    bool m_showWarnings;
};

rtu::RtuContext::Impl::Impl(RtuContext *parent)
    : QObject(parent)
    , m_owner(parent)
    , m_selectionModel(new ServersSelectionModel(this))

    , m_selection()
    , m_serversFinder(ServersFinder::create())
    , m_changesManager(new ChangesManager(
        parent, m_selectionModel, m_serversFinder.data(), this))

    , m_progressTask()
    , m_currentPageIndex(rtu::Constants::SettingsPage)
    , m_showWarnings(true)
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
    QObject::connect(m_serversFinder.data(), &ServersFinder::unknownAdded
        , m_selectionModel, &ServersSelectionModel::unknownAdded);
    QObject::connect(m_serversFinder.data(), &ServersFinder::unknownRemoved
        , m_selectionModel, &ServersSelectionModel::unknownRemoved);
    QObject::connect(m_serversFinder.data(), &ServersFinder::serverDiscovered
        , m_selectionModel, &ServersSelectionModel::serverDiscovered);

    QObject::connect(RestClient::instance(), &RestClient::accessMethodChanged
        , [this](const QUuid &id, bool byHttp) { m_selectionModel->changeAccessMethod(id, byHttp); });
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
    return new IpSettingsModel(m_selection.data());
}

rtu::TimeZonesModel *rtu::RtuContext::Impl::timeZonesModel(QObject *parent)
{
    return new TimeZonesModel(m_selectionModel->selectedServers(), parent);
}

rtu::ChangesManager *rtu::RtuContext::Impl::changesManager()
{
    return m_changesManager;
}

rtu::ApplyChangesTask *rtu::RtuContext::Impl::progressTask()
{
    return m_progressTask.get();
}

void rtu::RtuContext::Impl::showProgressTask(const ApplyChangesTaskPtr &task)
{
    changesManager()->clearChanges();
    if (m_progressTask == task)
        return;

    if (m_progressTask)
    {
        if (m_progressTask->inProgress())
        {
            /// if it is not minimized - minimize;
            changesManager()->changesProgressModel()->addChangeProgress(m_progressTask);
        }
        else
        {
            /// removes task
            changesManager()->changesProgressModel()->removeByTask(m_progressTask.get());
        }
    }

    m_progressTask = task;
    emit m_owner->progressTaskChanged();

    if (!task)
    {
        setCurrentPage(Constants::SettingsPage);
    }
    else if (task->inProgress())
    {
        setCurrentPage(Constants::ProgressPage);
    }
    else
    {
        selectionModel()->setSelectedItems(task->targetServerIds());
        setCurrentPage(Constants::SummaryPage);

        changesManager()->changesProgressModel()->removeByTask(task.get()); /// remove from list if task complete
    }
}

void rtu::RtuContext::Impl::applyTaskCompleted(const ApplyChangesTaskPtr &task)
{
    if (task.get() != progressTask())
        return;

    m_selectionModel->setSelectedItems(m_progressTask->targetServerIds());
    setCurrentPage(Constants::SummaryPage);
}

void rtu::RtuContext::Impl::showProgressTaskFromList(int index)
{
    const auto &task = changesManager()->changesProgressModel()->atIndex(index);
    showProgressTask(task);
}

void rtu::RtuContext::Impl::removeProgressTask(int index)
{
    const ApplyChangesTaskPtr &task = changesManager()->changesProgressModel()->atIndex(index);
    if (!task)
        return;

    if (task.get() == progressTask())
        m_owner->hideProgressTask();

    changesManager()->changesProgressModel()->removeByIndex(index);
}

void rtu::RtuContext::Impl::setCurrentPage(rtu::Constants::Pages pageId)
{
    if (m_currentPageIndex == pageId)
        return;
    
    m_currentPageIndex = pageId;

    emit m_owner->currentPageChanged();
}

bool rtu::RtuContext::Impl::showWarnings() const
{
    return m_showWarnings;
}

void rtu::RtuContext::Impl::setShowWarnings(bool show)
{
    m_showWarnings = show;
    emit m_owner->showWarningsChanged();
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

QObject *rtu::RtuContext::progressTask()
{
    return m_impl->progressTask();
}

void rtu::RtuContext::showProgressTask(const ApplyChangesTaskPtr &task)
{
    m_impl->showProgressTask(task);
}

void rtu::RtuContext::showProgressTaskFromList(int index)
{
    m_impl->showProgressTaskFromList(index);
}

void rtu::RtuContext::hideProgressTask()
{
    m_impl->showProgressTask(ApplyChangesTaskPtr());
}

void rtu::RtuContext::removeProgressTask(int index)
{
    m_impl->removeProgressTask(index);
}

void rtu::RtuContext::applyTaskCompleted(const ApplyChangesTaskPtr &task)
{
    m_impl->applyTaskCompleted(task);
}

bool isValidIpV4Address(const QString &address)
{
    const QHostAddress &ipHostAddress = QHostAddress(address);
    return (!ipHostAddress.isNull() 
        && (ipHostAddress.protocol() == QAbstractSocket::IPv4Protocol));
}

bool rtu::RtuContext::isValidSubnetMask(const QString &mask) const
{
    if (!isValidIpV4Address(mask))
        return false;

    quint32 value = QHostAddress(mask).toIPv4Address();
    value = ~value;
    return ((value & (value + 1)) == 0);
}

bool rtu::RtuContext::isDiscoverableFromCurrentNetwork(const QString &ip
    , const QString &mask) const
{
    if (!isValidSubnetMask(mask) || !isValidIpV4Address(ip))
        return false;

    const auto &allInterfaces = QNetworkInterface::allInterfaces();
    for(const auto &itf: allInterfaces)
    {
        const QNetworkInterface::InterfaceFlags flags = itf.flags();
        if (!itf.isValid() || !(flags & QNetworkInterface::IsUp)
            || !(flags & QNetworkInterface::IsRunning) 
            || (flags & QNetworkInterface::IsLoopBack))
        {
            continue;
        }

        for (const QNetworkAddressEntry &address: itf.addressEntries())
        {
            const QString &itfIp = address.ip().toString();
            const QString &itfMask = address.netmask().toString();
            if (!isValidIpV4Address(itfIp) || !isValidSubnetMask(itfMask))
                continue;

            if (isDiscoverableFromNetwork(ip, mask, itfIp, itfMask))
                return true;
        }
    }

    return false;
}

bool rtu::RtuContext::isDiscoverableFromNetwork(const QString &ip
    , const QString &mask
    , const QString &subnet
    , const QString &subnetMask) const
{
    if (!isValidIpV4Address(ip) || !isValidSubnetMask(mask)
        || !isValidIpV4Address(subnet) || !isValidSubnetMask(subnetMask))
    {
        return false;
    }

    const quint32 longMask = std::max(QHostAddress(mask).toIPv4Address()
        , QHostAddress(subnetMask).toIPv4Address());

    const quint32 ipNetwork = (QHostAddress(ip).toIPv4Address() & longMask );
    const quint32 secondNetwork = (QHostAddress(subnet).toIPv4Address() & longMask);

    return (ipNetwork == secondNetwork);
}


QDateTime rtu::RtuContext::applyTimeZone(const QDate &date
    , const QTime &time
    , const QByteArray &prevTimeZoneId
    , const QByteArray &newTimeZoneId)
{
    if (newTimeZoneId.isEmpty())
        return QDateTime();

    const QTimeZone prevTimeZone(!prevTimeZoneId.isEmpty() ? QTimeZone(prevTimeZoneId)
        : QDateTime::currentDateTime().timeZone());

    const QTimeZone nextTimeZone(newTimeZoneId);
    if (date.isNull() || time.isNull() || !prevTimeZone.isValid()
        || !nextTimeZone.isValid() || !date.isValid() || !time.isValid())
    {
        return QDateTime(date, time);
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

QString rtu::RtuContext::toolDisplayName() const
{
    return QString(QN_APPLICATION_DISPLAY_NAME);
}

bool rtu::RtuContext::isBeta() const
{
    return (QStringLiteral("true") == QStringLiteral(QN_BETA));
}

QString rtu::RtuContext::toolVersion() const
{
    return QString(QN_APPLICATION_VERSION);
}

QString rtu::RtuContext::toolRevision() const
{
    return QString(QN_APPLICATION_REVISION);
}

QString rtu::RtuContext::toolSupportMail() const
{
    return QString(QN_SUPPORT_MAIL_ADDRESS);
}

QString rtu::RtuContext::toolCompanyUrl() const
{
    return QString(QN_COMPANY_URL);
}

int rtu::RtuContext::currentPage() const
{
    return m_impl->currentPage();
}

bool rtu::RtuContext::showWarnings() const
{
    return m_impl->showWarnings();
}

void rtu::RtuContext::setShowWarnings(bool show)
{
    m_impl->setShowWarnings(show);
}

QObject *rtu::RtuContext::selection() const
{
    return m_impl->selection();
}

