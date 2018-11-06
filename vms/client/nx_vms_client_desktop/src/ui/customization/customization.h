#ifndef QN_CUSTOMIZATION_H
#define QN_CUSTOMIZATION_H

#include <QtCore/QMetaType>
#include <QtCore/QJsonObject>

class QnCustomization {
public:
    explicit QnCustomization(const QString &fileName = QString());

    bool isNull() const {
        return m_data.isEmpty();
    }

    const QJsonObject &data() const {
        return m_data;
    }

    void add(const QnCustomization &data);

private:
    QJsonObject m_data;
};

Q_DECLARE_METATYPE(QnCustomization)

#endif // QN_CUSTOMIZATION_H
