#ifndef QN_WORKBENCH_CUSTOMIZER_H
#define QN_WORKBENCH_CUSTOMIZER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QWeakPointer>

#include <client/client_model_types.h>

#include "workbench_context_aware.h"

class QnJsonSerializer;

class QnWorkbenchCustomizer: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

public:
    QnWorkbenchCustomizer(QObject *parent = NULL);
    virtual ~QnWorkbenchCustomizer();

    void customize(const QString &key, QObject *object);

private:
    QnWeakObjectHash m_objectByKey;
    QScopedPointer<QnJsonSerializer> m_serializer;
};


#endif // QN_WORKBENCH_CUSTOMIZER_H
