#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <QtCore/QHash>
#include <QtNetwork/QHostAddress>

#include <utils/common/singleton.h>
#include <utils/network/module_information.h>

class QnMulticastModuleFinder;
class QnDirectModuleFinder;
class QnDirectModuleFinderHelper;

class QnModuleFinder : public QObject, public Singleton<QnModuleFinder> {
    Q_OBJECT
public:
    QnModuleFinder(bool clientOnly);

    virtual ~QnModuleFinder();

    void setCompatibilityMode(bool compatibilityMode);

    QList<QnModuleInformation> foundModules() const;

    QnModuleInformation moduleInformation(const QString &moduleId) const;

    QnMulticastModuleFinder *multicastModuleFinder() const;
    QnDirectModuleFinder *directModuleFinder() const;

    /*! Hacky thing. This function emits moduleLost for every found module and then moduleFound for them.
     * It's used to force a server to drop all its connections and find new.
     */
    void makeModulesReappear();

public slots:
    void start();
    void stop();

signals:
    void moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress);
    void moduleLost(const QnModuleInformation &moduleInformation);

private slots:
    void at_moduleFound(const QnModuleInformation &moduleInformation, const QString &remoteAddress);
    void at_moduleLost(const QnModuleInformation &moduleInformation);

private:
    QnMulticastModuleFinder *m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnDirectModuleFinderHelper *m_directModuleFinderHelper;

    QHash<QnId, QnModuleInformation> m_foundModules;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
