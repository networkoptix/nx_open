#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include "../issue.h"

namespace nx::vms::server::interactive_settings {

class AbstractEngine;

} // namespace nx::vms::server::interactive_settings

namespace nx::vms::server::interactive_settings::components {

/** Base class for all items of the interactive settings engine. */
class Item: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type MEMBER m_type FINAL CONSTANT)
    Q_PROPERTY(QString name MEMBER m_name FINAL)
    Q_PROPERTY(QString caption MEMBER m_caption FINAL)
    Q_PROPERTY(QString description MEMBER m_description FINAL)
    Q_PROPERTY(bool visible MEMBER m_visible FINAL)
    Q_PROPERTY(bool enabled MEMBER m_enabled FINAL)

public:
    static QString kInterativeSettingsEngineProperty;

    AbstractEngine* engine() const;
    void setEngine(AbstractEngine* engine) { m_engine = engine; }

    QString type() const { return m_type; }
    QString name() const { return m_name; }
    QString caption() const { return m_caption; }
    QString description() const { return m_description; }
    bool visible() const { return m_visible; }
    bool enabled() const { return m_enabled; }

    virtual QJsonObject serializeModel() const;

protected:
    Item(const QString& type, QObject* parent = nullptr);

    void emitIssue(const Issue& issue) const;
    void emitIssue(Issue::Type type, Issue::Code code, const QString& message) const;
    void emitWarning(Issue::Code code, const QString& message) const;
    void emitError(Issue::Code code, const QString& message) const;

private:
    mutable AbstractEngine* m_engine = nullptr;
    const QString m_type;
    QString m_name;
    QString m_caption;
    QString m_description;
    bool m_visible = true;
    bool m_enabled = true;
};

} // namespace nx::vms::server::interactive_settings::components
