#pragma once

#include <QtCore/QObject>

#include <common/common_module_aware.h>

#include <utils/common/connective.h>
#include <network/base_system_description.h>

class QnAbstractSystemsFinder : public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnAbstractSystemsFinder(QObject *parent = nullptr);

    virtual ~QnAbstractSystemsFinder();

    typedef QList<QnSystemDescriptionPtr> SystemDescriptionList;
    virtual SystemDescriptionList systems() const = 0;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const = 0;

signals:
    void systemDiscovered(const QnSystemDescriptionPtr &system);

    void systemLost(const QString &id);

    void systemLostInternal(const QString &id, const QnUuid& localId);
};