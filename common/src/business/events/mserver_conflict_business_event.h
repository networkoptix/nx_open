#ifndef __MSERVER_CONFLICT_BUSINESS_EVENT_H__
#define __MSERVER_CONFLICT_BUSINESS_EVENT_H__

#include <QtCore/QString>
#include <QtCore/QStringList>

#include "conflict_business_event.h"
#include "core/resource/resource_fwd.h"

struct QnModuleInformation;

struct QnCameraConflictList {
    QString sourceServer;
    QHash<QString, QStringList> camerasByServer;

    QString encode() const;
    void decode(const QString &encoded);
};

class QnMServerConflictBusinessEvent: public QnConflictBusinessEvent
{
    typedef QnConflictBusinessEvent base_type;
public:
    QnMServerConflictBusinessEvent(const QnResourcePtr& mServerRes, qint64 timeStamp, const QnCameraConflictList& conflictList);
    QnMServerConflictBusinessEvent(const QnResourcePtr& mServerRes, qint64 timeStamp, const QnModuleInformation &conflictModule, const QUrl &conflictUrl);

private:
    QnCameraConflictList m_cameraConflicts;
};

typedef QSharedPointer<QnMServerConflictBusinessEvent> QnMServerConflictBusinessEventPtr;

#endif // __MSERVER_CONFLICT_BUSINESS_EVENT_H__
