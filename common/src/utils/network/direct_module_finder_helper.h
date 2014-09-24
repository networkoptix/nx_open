#ifndef DIRECT_MODULE_FINDER_HELPER_H
#define DIRECT_MODULE_FINDER_HELPER_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QPointer>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <utils/common/id.h>

typedef QList<QHostAddress> QnHostAddressList;
typedef QSet<QHostAddress> QnHostAddressSet;
typedef QSet<QUrl> QnUrlSet;
class QnModuleFinder;
class QnDirectModuleFinder;
class QnMulticastModuleFinder;

class QnModuleFinderHelper : public QObject {
    Q_OBJECT
public:
    explicit QnModuleFinderHelper(QnModuleFinder *moduleFinder);

    QnUrlSet urlsForPeriodicalCheck() const;
    void setUrlsForPeriodicalCheck(const QnUrlSet &urls);

private slots:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);
    void at_resourceAuxUrlsChanged(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_timer_timeout();

private:
    QnMulticastModuleFinder *m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QHash<QUuid, QnHostAddressSet> m_addressesByServer;
    QHash<QUuid, QnUrlSet> m_manualAddressesByServer;
    QHash<QUuid, quint16> m_portByServer;
    QnUrlSet m_urlsForPeriodicalCheck;
};

#endif // DIRECT_MODULE_FINDER_HELPER_H
