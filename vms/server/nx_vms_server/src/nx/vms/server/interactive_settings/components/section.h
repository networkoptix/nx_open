#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

class Section;
class SectionContainer: public Group
{
    Q_OBJECT
    using base_type = Group;

public:
    using base_type::base_type; //< Forward constructors.

    QQmlListProperty<Section> sections();
    const QList<Section*> sectionList() const;

    virtual QJsonObject serializeModel() const override;

private:
    QList<Section*> m_sections;
};

/**
 * Section is represented as a separate page of settings. Sections are accessible from an external
 * menu (in the client it is displayed in the sidebar).
 */
class Section: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Section(QObject* parent = nullptr);
};

/**
 * This item must be used as a root item for all settings.
 */
class Settings: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Settings(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
