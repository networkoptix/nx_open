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

    void forbidTileAction();

signals:
    /**
     * Forces tile action:
     * - in case of factory systems we have to connect to it;
     * - in case of cloud system (and connection to cloud presence)
     *   or systems with saved password - we have to connect to system;
     * - otherwise just expand tile
     */

    void tileAction(const QString& systemId, bool initial);

private:
    void handleFirstSystems();

    void throwTileAction(const QnSystemDescriptionPtr& system
    , bool initial = false);

    void cancelAndUninitialize();

private:
    bool m_firstWaveHandled;
    bool m_actionThrown;
};

#define qnStartupTileManager QnStartupTileManager::instance()