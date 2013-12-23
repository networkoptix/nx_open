#ifndef QN_CUSTOMIZER_H
#define QN_CUSTOMIZER_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>

#include "customization.h"

class QnJsonSerializer;

class QnCustomizer: public QObject, public Singleton<QnCustomizer> {
    Q_OBJECT
public:
    QnCustomizer(QObject *parent = NULL);
    QnCustomizer(const QnCustomization &customization, QObject *parent = NULL);
    virtual ~QnCustomizer();

    void setCustomization(const QnCustomization &customization);
    const QnCustomization &customization() const;

    void customize(QObject *object);

private:
    void customize(QObject *object, const QJsonObject &customization, const QString &key);

private:
    QScopedPointer<QnJsonSerializer> m_serializer;
    QnCustomization m_customization;
};

#define qnCustomizer (QnCustomizer::instance())


#endif // QN_CUSTOMIZER_H
