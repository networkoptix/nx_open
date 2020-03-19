#include "item.h"

#include <QtQml/QtQml>

#include <nx/utils/log/log.h>

#include "../abstract_engine.h"

namespace nx::vms::server::interactive_settings::components {

QString Item::kInterativeSettingsEngineProperty = "_nx_interactive_settings_engine";

Item::Item(const QString& type, QObject* parent):
    QObject(parent),
    m_type(type)
{
}

void Item::emitIssue(const Issue& issue) const
{
    if (engine())
        engine()->addIssue(issue);
    else
        NX_DEBUG(this, "%1: %2", issue.code, issue.message);
}

void Item::emitIssue(Issue::Type type, Issue::Code code, const QString& message) const
{
    emitIssue(Issue(type, code,
        QStringLiteral("[%1 \"%2\"] ").arg(this->type(), name()) + message));
}

void Item::emitWarning(Issue::Code code, const QString& message) const
{
    emitIssue(Issue::Type::warning, code, message);
}

void Item::emitError(Issue::Code code, const QString& message) const
{
    emitIssue(Issue::Type::error, code, message);
}

AbstractEngine* Item::engine() const
{
    if (!m_engine)
    {
        if (const auto context = qmlContext(this); context && context->isValid())
        {
            m_engine = dynamic_cast<AbstractEngine*>(
                context->contextProperty(kInterativeSettingsEngineProperty).value<QObject*>());
        }
    }
    return m_engine;
}

QJsonObject Item::serializeModel() const
{
    QJsonObject result;
    result[QLatin1String("type")] = m_type;
    if (!m_name.isEmpty())
        result[QLatin1String("name")] = m_name;
    if (!m_caption.isEmpty())
        result[QLatin1String("caption")] = m_caption;
    if (!m_description.isEmpty())
        result[QLatin1String("description")] = m_description;
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
