#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>
#include <QtCore/QUrl>

namespace nx::mediaserver::interactive_settings {

namespace components { class Settings; }

class AbstractEngine: public QObject
{
    using base_type = QObject;

public:
    enum class Status
    {
        idle,
        loaded,
        error,
    };

    AbstractEngine(QObject* parent = nullptr);
    virtual ~AbstractEngine() override;

    virtual void load(const QByteArray& data) = 0;
    virtual void load(const QString& fileName);

    QJsonObject serialize() const;
    QVariantMap values() const;

    void applyValues(const QVariantMap& values) const;
    QJsonObject tryValues(const QVariantMap& values) const;

    Status status() const;

    QObject* rootObject() const;

protected:
    void setStatus(Status status);
    components::Settings* settingsItem() const;
    void setSettingsItem(components::Settings* item);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::mediaserver::interactive_settings
