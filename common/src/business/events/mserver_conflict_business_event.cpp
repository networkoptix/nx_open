#include "mserver_conflict_business_event.h"
#include "core/resource/resource.h"

namespace QnBusinessEventRuntime {

    static QLatin1String currentECStr("currentEC");
    static QLatin1String conflictECsStr("conflictECs");

    static QLatin1String separator("\n");

   QString getCurrentEC(const QnBusinessParams &params) {
        return params.value(currentECStr, QString()).toString();
    }

    void setCurrentEC(QnBusinessParams* params, QString value) {
        (*params)[currentECStr] = value;
    }

    QStringList getConflictECs(const QnBusinessParams &params) {
        return params.value(conflictECsStr, QString()).toString().split(separator, QString::SkipEmptyParts);
    }

    void setConflictECs(QnBusinessParams* params, QStringList value) {
        (*params)[conflictECsStr] = value.join(separator);
    }

}

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QList<QByteArray>& conflictList):
    base_type(BusinessEventType::BE_MediaServer_Conflict,
                            mServerRes,
                            timeStamp),
    m_conflictList(conflictList)
{
}

QString QnMServerConflictBusinessEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    if (!m_conflictList.isEmpty()) {
        text += QObject::tr("  several media servers are running on different EC! current EC connection %1 conflict with EC(s): ").arg(QString::fromUtf8(m_conflictList[0]));
        for (int i = 1; i < m_conflictList.size(); ++i) {
            if (i > 1)
                text += QLatin1String(", ");
            text += QString::fromLocal8Bit(m_conflictList[i]);
        }
    }
    return text;
}

QnBusinessParams QnMServerConflictBusinessEvent::getRuntimeParams() const {
    QnBusinessParams params = base_type::getRuntimeParams();

    if (!m_conflictList.isEmpty()) {
        QnBusinessEventRuntime::setCurrentEC(&params, QString::fromUtf8(m_conflictList[0]));

        QStringList conflictECs;
        for (int i = 1; i < m_conflictList.size(); i++)
            conflictECs << QString::fromUtf8(m_conflictList[i]);
        QnBusinessEventRuntime::setConflictECs(&params, conflictECs);
    }

    return params;
}
