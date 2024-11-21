// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

#include "attribute_display_manager.h"

namespace nx::vms::client::desktop::analytics {

/**
 * Displayed attributes filter.
 * To be used in QML to preprocess attributes before feeding them to AnalyticsAttributeTable.
 */
class AttributeFilter: public QObject
{
    Q_OBJECT
    using Attribute = nx::vms::client::core::analytics::Attribute;
    using AttributeList = nx::vms::client::core::analytics::AttributeList;
    using Manager = nx::vms::client::desktop::analytics::taxonomy::AttributeDisplayManager;

    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AttributeDisplayManager* manager
        READ manager WRITE setManager NOTIFY managerChanged)

    Q_PROPERTY(nx::vms::client::core::analytics::AttributeList attributes
        READ attributes WRITE setAttributes NOTIFY attributesChanged)

    Q_PROPERTY(bool filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool sort READ sort WRITE setSort NOTIFY sortChanged)

    Q_PROPERTY(nx::vms::client::core::analytics::AttributeList displayedAttributes
        READ displayedAttributes NOTIFY displayedAttributesChanged)

public:
    explicit AttributeFilter(QObject* parent = nullptr);
    virtual ~AttributeFilter() override;

    static void registerQmlType();

    /** Externally owned display manager to control attribute filtering and sorting. */
    Manager* manager() const;
    void setManager(Manager* value);

    /** Source attributes. */
    AttributeList attributes() const;
    void setAttributes(const AttributeList& value);

    /** Whether to apply filter. */
    bool filter() const;
    void setFilter(bool value);

    /** Whether to apply sorting. */
    bool sort() const;
    void setSort(bool value);

    /** Filtered and/or sorted result. */
    AttributeList displayedAttributes() const;

signals:
    void managerChanged();
    void attributesChanged();
    void filterChanged();
    void sortChanged();
    void displayedAttributesChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics
