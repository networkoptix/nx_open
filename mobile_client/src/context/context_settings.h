#pragma once

#include <QtCore/QObject>

class QnContextSettingsPrivate;
class QnContextSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString systemId READ systemId WRITE setSystemId NOTIFY systemIdChanged)

public:
    explicit QnContextSettings(QObject* parent = nullptr);
    ~QnContextSettings();

    QString systemId() const;
    void setSystemId(const QString& systemId);

signals:
    void systemIdChanged();

private:
    Q_DECLARE_PRIVATE(QnContextSettings)
    QScopedPointer<QnContextSettingsPrivate> d_ptr;
};
