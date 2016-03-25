
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <network/system_description.h>

class QnAbstractSystemsFinder : public Connective<QObject>
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
};