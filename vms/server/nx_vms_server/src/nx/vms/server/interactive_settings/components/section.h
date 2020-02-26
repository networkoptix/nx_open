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

    virtual QJsonObject serialize() const override;

private:
    QList<Section*> m_sections;
};

class Section: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Section(QObject* parent = nullptr);
};

class Settings: public SectionContainer
{
    Q_OBJECT
    using base_type = SectionContainer;

public:
    Settings(QObject* parent = nullptr);
};

} // namespace nx::vms::server::interactive_settings::components
