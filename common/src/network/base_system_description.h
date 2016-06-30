
#pragma once


#pragma once

#include <QtCore/QObject>

#include <network/module_information.h>
#include <nx/utils/uuid.h>

class QnBaseSystemDescription;
typedef QSharedPointer<QnBaseSystemDescription> QnSystemDescriptionPtr;

enum class QnServerField
{
    NoField = 0x00
    , NameField = 0x01
    , SystemNameField = 0x02
    , HostField = 0x04
    , FlagsField = 0x08
};
Q_DECLARE_FLAGS(QnServerFields, QnServerField)
Q_DECLARE_METATYPE(QnServerFields)

class QnBaseSystemDescription : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnBaseSystemDescription()
        : base_type() {}

    virtual ~QnBaseSystemDescription() {}

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual QString ownerAccountEmail() const = 0;

    virtual QString ownerFullName() const = 0;

    virtual bool isCloudSystem() const = 0;

    typedef QList<QnModuleInformation> ServersList;
    virtual ServersList servers() const = 0;

    virtual bool containsServer(const QnUuid &serverId) const = 0;

    virtual QnModuleInformation getServer(const QnUuid &serverId) const = 0;

    virtual QString getServerHost(const QnUuid &serverId) const = 0;

    virtual qint64 getServerLastUpdatedMs(const QnUuid &serverId) const = 0;

signals:
    void idChanged();

    void isCloudSystemChanged();

    void ownerChanged();

    void serverAdded(const QnUuid &serverId);

    void serverRemoved(const QnUuid &serverId);

    void serverChanged(const QnUuid &serverId, QnServerFields flags);
};