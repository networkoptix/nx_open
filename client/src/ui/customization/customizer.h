#ifndef QN_CUSTOMIZER_H
#define QN_CUSTOMIZER_H

#include <QtCore/QObject>

#include <nx/tool/singleton.h>

#include "customization.h"

class QnJsonSerializer;

class QnCustomizerPrivate;

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
    QScopedPointer<QnCustomizerPrivate> d;
};

#define qnCustomizer (QnCustomizer::instance())


#endif // QN_CUSTOMIZER_H
