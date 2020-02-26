#include "section.h"

#include <QtCore/QJsonArray>

namespace nx::vms::server::interactive_settings::components {

QQmlListProperty<Section> SectionContainer::sections()
{
    return QQmlListProperty<Section>(this, m_sections);
}

const QList<Section*> SectionContainer::sectionList() const
{
    return m_sections;
}

QJsonObject SectionContainer::serialize() const
{
    auto result = base_type::serialize();
    if (m_sections.empty())
        return result;

    QJsonArray sections;
    for (const auto section: m_sections)
        sections.append(section->serialize());

    result[QStringLiteral("sections")] = sections;
    return result;
}

Section::Section(QObject* parent):
    base_type(QStringLiteral("Section"), parent)
{
}

Settings::Settings(QObject* parent):
    base_type(QStringLiteral("Settings"), parent)
{
}

} // namespace nx::vms::server::interactive_settings::components
