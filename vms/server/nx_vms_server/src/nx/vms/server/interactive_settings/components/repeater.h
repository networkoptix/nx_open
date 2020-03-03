#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

/**
 * A generator of similar items or item structures from a template.
 *
 * In the GUI it displays the minimum possible amount of items (from the first to the last filled
 * item) and a button "Add" which should open one more item.
 */
class Repeater: public Group
{
    Q_OBJECT
    Q_PROPERTY(QJsonValue template READ itemTemplate WRITE setItemTemplate
        NOTIFY itemTemplateChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int startIndex READ startIndex WRITE setStartIndex NOTIFY startIndexChanged)
    Q_PROPERTY(QString addButtonCaption READ addButtonCaption WRITE setAddButtonCaption
        NOTIFY addButtonCaptionChanged)

    using base_type = Group;

public:
    Repeater(QObject* parent = nullptr);

    /**
     * The template items are generated from.
     *
     * To distinguish items from each other, Repeater substitutes index in the template object
     * values. The following strings are considered as index placeholders (note that Repeaters
     * could be nested, in other words the template could contain another Repeater):
     *     - `#N` (where N is a number >= 1): Index in a Repeater of depth N
     *     - `#{N}`: Another syntax which allows to write numbers right after the index.
     *         E.g. 10#{1}10.
     *     - `#`: A shortcut for index of a Repeater of depth 1 (#1).
     * Sinse `#` is an index placeholder, the `#` sign can be escaped by doubling it: `##` will
     * be replaced with `#`.
     *
     * Repeater on **every level of depth** performs the substitution through the whole template
     * object structure. Thus the deepest parts of the template could be processed multiple times:
     * one time per Repeater. This must be taken into account when a `#` sign is needed to be
     * escaped in an inner Repeater. Every outer Repeater will replace `##` with `#`. So if there
     * must be a resulting string `#` in an item of a Repeater of depth 3, it should be `########`
     * in the original JSON.
     */
    QJsonValue itemTemplate() const { return m_itemTemplate; }
    void setItemTemplate(const QJsonValue& itemTemplate);

    /** Amount of items to be generated. */
    int count() const { return m_count; }
    void setCount(int count);

    /** Index of the first item (default: 1). */
    int startIndex() const { return m_startIndex; }
    void setStartIndex(int index);

    QString addButtonCaption() const { return m_addButtonCaption; }
    void setAddButtonCaption(const QString& caption);

    virtual QJsonObject serializeModel() const override;

signals:
    void itemTemplateChanged();
    void countChanged();
    void startIndexChanged();
    void addButtonCaptionChanged();

private:
    QJsonObject m_itemTemplate;
    int m_count = 0;
    int m_startIndex = 1;
    QString m_addButtonCaption;
};

} // namespace nx::vms::server::interactive_settings::components
