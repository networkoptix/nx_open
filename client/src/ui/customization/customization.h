#ifndef QN_CUSTOMIZATION_H
#define QN_CUSTOMIZATION_H

#include <QtCore/QMetaType>
#include <QtCore/QVariant>

class QApplication;

// TODO: #Elric:
// use three customization files:
// 
// customization_common
// customization_base
// customization_child
// 
// Add QnCustomization::add(const QnCustomization &other) method that would merge'em.

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

    void add(const QnCustomization &data);

private:
    QJsonObject m_data;
};

Q_DECLARE_METATYPE(QnCustomization)

#endif // QN_CUSTOMIZATION_H
