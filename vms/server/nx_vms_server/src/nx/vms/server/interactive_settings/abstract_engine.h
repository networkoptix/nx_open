#pragma once

#include <memory>

#include <QtCore/QJsonObject>

#include "issue.h"

namespace nx::vms::server::interactive_settings {

namespace components {

class Item;
class Settings;

} // namespace components

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
class AbstractEngine: public QObject
{
public:
    AbstractEngine();
    virtual ~AbstractEngine();

    virtual bool loadModelFromData(const QByteArray& data) = 0;
    virtual bool loadModelFromFile(const QString& fileName);

    QJsonObject serializeModel() const;

    /**
     * @return Map from value name to a typed value.
     */
    QJsonObject values() const;

    /**
     * @param values Map from value name to a typed value.
     */
    void applyValues(const QJsonObject& values);

    /**
     * @param values Map from value name to a string value representation.
     * This method does not emit value conversion warnings.
     */
    void applyStringValues(const QJsonObject& values);

    bool skipStringConversionWarnings() const { return m_skipStringConversionWarnings; }

    struct ModelAndValues
    {
        QJsonObject model;
        QJsonObject values;
    };

    /**
     * Applies the values, computes dependent properties, returns corrected model and values, and
     * finally rolls back the changes to the stored model and values.
     * @param values Map from value map to either a typed value or a string value representation.
     */
    ModelAndValues tryValues(const QJsonObject& values);

    /*
     * This is a hint for items that the engine is updating values of items at the moment and items
     * should accept values as is without any correction. See ValueItem::applyConstraints().
     */
    bool updatingValues() const { return m_updatingValues; }

    /*
     * @return The list of warnings and errors of the last command (load/apply values).
     */
    QList<Issue> issues() const;

    void addIssue(const Issue& issue);

    /*
     * @return True if there are errors in the issues list.
     */
    bool hasErrors() const;

protected:
    components::Settings* settingsItem() const;
    bool setSettingsItem(std::unique_ptr<components::Item> item);

    /* Apply initial default values to all items. */
    void initValues();

    /** See updatingValues(). */
    inline void startUpdatingValues() { m_updatingValues = true; }

    /** See updatingValues(). */
    inline void stopUpdatingValues() { m_updatingValues = false; }

    void clearIssues();

private:
    std::unique_ptr<components::Settings> m_settingsItem;
    QList<Issue> m_issues;
    bool m_updatingValues = false;
    bool m_skipStringConversionWarnings = false;
};

} // namespace nx::vms::server::interactive_settings
