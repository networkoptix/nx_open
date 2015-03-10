#ifndef NETWORKOPTIXMODULEFINDER_H
#define NETWORKOPTIXMODULEFINDER_H

#include <QtCore/QHash>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <utils/common/singleton.h>
#include <utils/network/module_information.h>
#include <core/resource/resource_fwd.h>

class QnMulticastModuleFinder;
class QnDirectModuleFinder;
class QnDirectModuleFinderHelper;
class QTimer;

class QnModuleFinder : public QObject, public Singleton<QnModuleFinder> {
    Q_OBJECT
public:
    QnModuleFinder(bool clientOnly);

    virtual ~QnModuleFinder();

    void setCompatibilityMode(bool compatibilityMode);
    bool isCompatibilityMode() const;

    QList<QnModuleInformation> foundModules() const;

    QnModuleInformation moduleInformation(const QnUuid &moduleId) const;

    QnMulticastModuleFinder *multicastModuleFinder() const;
    QnDirectModuleFinder *directModuleFinder() const;

    int pingTimeout() const;

public slots:
    void start();
    void pleaseStop();

signals:
    void moduleChanged(const QnModuleInformation &moduleInformation);
    void moduleLost(const QnModuleInformation &moduleInformation);
    void moduleUrlFound(const QnModuleInformation &moduleInformation, const QUrl &url);
    void moduleUrlLost(const QnModuleInformation &moduleInformation, const QUrl &url);
    void moduleConflict(const QnModuleInformation &moduleInformation, const QUrl &url);

private slots:
    void at_responseReceived(QnModuleInformationEx moduleInformation, QUrl url);
    void at_timer_timeout();

private:
    QSet<QString> moduleAddresses(const QnUuid &id) const;
    void removeUrl(const QUrl &url);
    void addUrl(const QUrl &url, const QnUuid &id);
    void handleSelfResponse(const QnModuleInformationEx &moduleInformation, const QUrl &url);

private:
    QElapsedTimer m_elapsedTimer;
    QTimer *m_timer;

    QScopedPointer<QnMulticastModuleFinder> m_multicastModuleFinder;
    QnDirectModuleFinder *m_directModuleFinder;
    QnDirectModuleFinderHelper *m_helper;

    QHash<QnUuid, QnModuleInformationEx> m_foundModules;
    QMultiHash<QnUuid, QUrl> m_urlById;
    QHash<QUrl, QnUuid> m_idByUrl;
    QHash<QUrl, qint64> m_lastResponse;
    QHash<QnUuid, qint64> m_lastResponseById;
    QHash<QnUuid, qint64> m_lastConflictResponseById;
    QHash<QnUuid, int> m_conflictResponseCountById;

    qint64 m_lastSelfConflict;
    int m_selfConflictCount;
};

#endif  //NETWORKOPTIXMODULEFINDER_H
