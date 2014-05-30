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
class QnDirectModuleFinder;

class QnDirectModuleFinderHelper : public QObject {
    Q_OBJECT
public:
    explicit QnDirectModuleFinderHelper(QObject *parent = 0);

    void setDirectModuleFinder(QnDirectModuleFinder *directModuleFinder);

private slots:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);

private:
    QPointer<QnDirectModuleFinder> m_directModuleFinder;
    QHash<QnId, QnHostAddressSet> m_addressesByServer;
    QHash<QnId, quint16> m_portByServer;
};

#endif // DIRECT_MODULE_FINDER_HELPER_H
