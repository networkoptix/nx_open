#ifndef QN_CUSTOMIZER_H
#define QN_CUSTOMIZER_H

#include <QtCore/QObject>

class QnJsonSerializer;

class QnCustomizer: public QObject {
    Q_OBJECT
public:
    QnCustomizer(QObject *parent = NULL);
    virtual ~QnCustomizer();

    void setCustomization(const QString &customizationFileName);
    void setCustomization(const QVariantMap &customization);
    const QVariantMap &customization() const;

    void customize(QObject *object);
    void customize(QObject *object, const QString &key);

private:
    QVariantMap m_customization;
    QScopedPointer<QnJsonSerializer> m_serializer;
};


#endif // QN_CUSTOMIZER_H
