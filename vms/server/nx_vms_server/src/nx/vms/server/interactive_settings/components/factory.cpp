#include "factory.h"

#include <QtQml/QtQml>

#include <nx/utils/thread/mutex.h>

#include "section.h"
#include "repeater.h"
#include "group_box.h"
#include "row.h"

#include "text_field.h"
#include "text_area.h"
#include "spin_box.h"
#include "check_box.h"
#include "check_box_group.h"
#include "radio_button_group.h"
#include "combo_box.h"
#include "roi_items.h"

#include "button.h"

#include "separator.h"

namespace nx::vms::server::interactive_settings::components {

namespace {

using ItemInstantiator = std::function<Item*(QObject*)>;
QHash<QString, ItemInstantiator> itemInstantiators;
const char* kUri = "nx.mediaserver.interactive_settings";
utils::Mutex mutex;

template<typename T>
void registerType(const char* name)
{
    qmlRegisterType<T>(kUri, 1, 0, name);
    itemInstantiators.insert(name, [](QObject* parent) { return new T(parent); });
}

template<typename T>
void registerUncreatableType(const char* name)
{
    qmlRegisterUncreatableType<T>(kUri, 1, 0, name,
        QStringLiteral("Cannot create object of type %1").arg(name));
}

} // namespace

void Factory::registerTypes()
{
    NX_MUTEX_LOCKER lock(&mutex);

    if (!itemInstantiators.isEmpty())
        return;

    registerUncreatableType<Item>("Item");
    registerUncreatableType<ValueItem>("ValueItem");
    registerUncreatableType<EnumerationItem>("EnumerationItem");
    registerUncreatableType<IntegerNumberItem>("IntegerNumberItem");

    registerType<Settings>("Settings");
    registerType<GroupBox>("GroupBox");
    registerType<Section>("Section");
    registerType<Repeater>("Repeater");
    registerType<Row>("Row");
    registerType<TextField>("TextField");
    registerType<TextArea>("TextArea");
    registerType<ComboBox>("ComboBox");
    registerType<SpinBox>("SpinBox");
    registerType<DoubleSpinBox>("DoubleSpinBox");
    registerType<CheckBox>("CheckBox");
    registerType<Button>("Button");
    registerType<Separator>("Separator");
    registerType<RadioButtonGroup>("RadioButtonGroup");
    registerType<CheckBoxGroup>("CheckBoxGroup");

    registerType<LineFigure>("LineFigure");
    registerType<BoxFigure>("BoxFigure");
    registerType<PolygonFigure>("PolygonFigure");
    registerType<ObjectSizeConstraints>("ObjectSizeConstraints");
}

Item* Factory::createItem(const QString& type, QObject* parent)
{
    if (auto instantiator = itemInstantiators.value(type))
        return instantiator(parent);
    return nullptr;
}

} // namespace nx::vms::server::interactive_settings::components
