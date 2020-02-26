#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

namespace nx::vms::server::interactive_settings::components {

class Item: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type MEMBER m_type FINAL CONSTANT)
    Q_PROPERTY(QString name MEMBER m_name FINAL)
    Q_PROPERTY(QString caption MEMBER m_caption FINAL)
    Q_PROPERTY(QString description MEMBER m_description FINAL)

public:
    QString type() const
    {
        return m_type;
    }

    QString name() const
    {
        return m_name;
    }

    QString caption() const
    {
        return m_caption;
    }

    QString description() const
    {
        return m_description;
    }

    virtual QJsonObject serialize() const;

protected:
    Item(const QString& type, QObject* parent = nullptr);

private:
    const QString m_type;
    QString m_name;
    QString m_caption;
    QString m_description;
};

} // namespace nx::vms::server::interactive_settings::components
