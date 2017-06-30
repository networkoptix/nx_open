#pragma once

#include <QtCore/QObject>
#include <nx/utils/singleton.h>
#include <network/base_system_description.h>

class QnStartupTileManager: public QObject, public Singleton<QnStartupTileManager>
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnStartupTileManager();

    virtual ~QnStartupTileManager() = default;

    void skipTileAction();

signals:
    /**
     * Forces tile action:
     * - in case of factory systems we have to connect to it;
     * - in case of systems with saved password - we have to connect to system;
     * - in case of cloud systems do nothing (as for now)
     * - otherwise just expand tile
     */
    void tileActionRequested(const QString& systemId, bool initial);

private:
    void handleFirstSystems();

    void emitTileAction(
        const QnSystemDescriptionPtr& system,
        bool initial = false);

    void cancelAndUninitialize();

private:
    bool m_actionEmitted;
};

#define qnStartupTileManager QnStartupTileManager::instance()