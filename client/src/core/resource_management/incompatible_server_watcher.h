#ifndef INCOMPATIBLE_SERVER_WATCHER_H
#define INCOMPATIBLE_SERVER_WATCHER_H

#include <QtCore/QObject>
#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>
#include <utils/network/module_information.h>

class QnIncompatibleServerWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerWatcher(QObject *parent = 0);
    ~QnIncompatibleServerWatcher();

    void start();
    void stop();

    /*! Prevent incompatible resource from removal if it goes offline.
     * \param id Real server GUID.
     * \param keep true to keep the resource (if present), false to not keep.
     * In case when the resource was removed when was having 'keep' flag on
     * it will be removed in this method.
     */
    void keepServer(const QnUuid &id, bool keep);

    void createModules(const QList<QnModuleInformationWithAddresses> &modules);

private:
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);
    void at_moduleChanged(const QnModuleInformationWithAddresses &moduleInformation, bool isAlive);

    void addResource(const QnModuleInformationWithAddresses &moduleInformation);
    void removeResource(const QnUuid &id);
    QnUuid getFakeId(const QnUuid &realId) const;

private:
    struct ModuleInformationItem {
        QnModuleInformationWithAddresses moduleInformation;
        bool keep;
        bool removed;

        ModuleInformationItem() :
            keep(false),
            removed(false)
        {}

        ModuleInformationItem(const QnModuleInformationWithAddresses &moduleInformation) :
            moduleInformation(moduleInformation),
            keep(false),
            removed(false)
        {}
    };

    mutable QMutex m_mutex;
    QHash<QnUuid, QnUuid> m_fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> m_serverUuidByFakeUuid;
    QHash<QnUuid, ModuleInformationItem> m_moduleInformationById;
};

#endif // INCOMPATIBLE_SERVER_WATCHER_H
