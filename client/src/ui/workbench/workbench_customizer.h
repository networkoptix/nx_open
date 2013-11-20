#ifndef QN_WORKBENCH_CUSTOMIZER_H
#define QN_WORKBENCH_CUSTOMIZER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QWeakPointer>

#include "workbench_context_aware.h"

typedef 

class QnWorkbenchCustomizer: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

public:
    QnWorkbenchCustomizer(QObject *parent = NULL);
    virtual ~QnWorkbenchCustomizer();

    void registerObject(const QString &key, QObject *object);
    void unregisterObject(const QString &key);

    void customize(const QVariant &cusomization);

private:
    QnJsonSerializer *serializer(int type);

private:
    QHash<QString, QWeakPointer<QObject> > m_objectByKey;
};


#endif // QN_WORKBENCH_CUSTOMIZER_H
