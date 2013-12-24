#ifndef QN_CUSTOMIZATION_H
#define QN_CUSTOMIZATION_H

#include <QtCore/QMetaType>
#include <QtCore/QVariant>

class QApplication;

class QnCustomization {
public:
    QnCustomization();
    explicit QnCustomization(const QString &fileName);

    bool isNull() const {
        return m_data.isEmpty();
    }

    const QJsonObject &data() const {
        return m_data;
    }

private:
    QJsonObject m_data;
};

Q_DECLARE_METATYPE(QnCustomization)

#endif // QN_CUSTOMIZATION_H
