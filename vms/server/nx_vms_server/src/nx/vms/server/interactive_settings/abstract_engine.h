#pragma once

#include <QtCore/QPointer>
#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

namespace nx::vms::server::interactive_settings {

namespace components { class Settings; }

/**
 * Base class for interactive settings engines.
 *
 * The engine represents a model of interactive items. The model can be searialized to a simple
 * JSON suitable for building GUI from it in client applications. Interactive items can be divided
 * into the following categories:
 *     - Value items: items which have value (like TextField, SpinBox)
 *     - Triggers (so far this is only Button)
 *     - Grouping items (like GroupBox or Row)
 * All value items and triggers have unique names. For value items, the engine provides a values
 * map. Some items constrain their values (e.g. min and max value of SpinBox). Also certain engines
 * (like QML engine) may allow dependencies between item properties (e.g. min value of a SpinBox
 * may depend on the value of a CheckBox). Engine handles such dependencies internally, no support
 * from the client side is needed. Instead, after changing the values, a client application need to
 * send items values to the engine, request an updated JSON model, and apply the new constraints to
 * the created GUI controls.
 */
class AbstractEngine
{
public:
    enum class ErrorCode
    {
        ok,
        cannotOpenFile,
        fileIsTooLarge,
        itemNameIsNotUnique,
        parseError,
    };

    struct Error
    {
        ErrorCode code = ErrorCode::ok;
        QString message;

        Error(ErrorCode code, const QString& message = QString()): code(code), message(message) {}
    };

    AbstractEngine();
    virtual ~AbstractEngine();

    virtual Error loadModelFromData(const QByteArray& data) = 0;
    virtual Error loadModelFromFile(const QString& fileName);

    QJsonObject serializeModel() const;

    /**
     * @return Map from value name to a typed value.
     */
    QVariantMap values() const;

    /**
     * @param values Map from value map to either a typed value or a string value representation.
     */
    void applyValues(const QVariantMap& values) const;

    struct ModelAndValues
    {
        QJsonObject model;
        QVariantMap values;
    };

    /**
     * Applies the values, computes dependent properties, returns corrected model and values, and
     * finally rolls back the changes to the stored model and values.
     * @param values Map from value map to either a typed value or a string value representation.
     */
    ModelAndValues tryValues(const QVariantMap& values) const;

protected:
    components::Settings* settingsItem() const;
    Error setSettingsItem(components::Settings* item);

private:
    QScopedPointer<components::Settings> m_settingsItem;
};

} // namespace nx::vms::server::interactive_settings
