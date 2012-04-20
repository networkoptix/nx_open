#ifndef QN_ACTION_PARAMETERS_H
#define QN_ACTION_PARAMETERS_H

#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>

#include "action_fwd.h"
#include "actions.h"

class QnActionParameters {
public:
    QnActionParameters(const QVariantMap &arguments = QVariantMap());

    explicit QnActionParameters(const QVariant &items, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnResourcePtr &resource, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnResourceList &resources, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QList<QGraphicsItem *> &items, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnResourceWidgetList &widgets, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnWorkbenchLayoutList &layouts, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnLayoutItemIndexList &layoutItems, const QVariantMap &arguments = QVariantMap());

    const QVariant &items() const {
        return m_items;
    }

    Qn::ActionTargetType itemsType() const;

    int itemsSize() const;
    
    QnResourceList resources() const;

    QnResourcePtr resource() const;

    QnLayoutItemIndexList layoutItems() const;

    QnWorkbenchLayoutList layouts() const;

    QnResourceWidgetList widgets() const;

    void setItems(const QVariant &items);

    const QVariantMap &arguments() const {
        return m_arguments;
    }

    QVariant argument(const QString &key) const {
        return m_arguments.value(key);
    }

    template<class T>
    T argument(const QString &key) const {
        return m_arguments.value(key).value<T>();
    }

    bool hasArgument(const QString &key) const {
        return m_arguments.contains(key);
    }

    void setArguments(const QVariantMap &arguments) {
        m_arguments = arguments;
    }

    void setArgument(const QString &key, const QVariant &value) {
        m_arguments[key] = value;
    }

    template<class T>
    void setArgument(const QString &key, const T &value) {
        m_arguments[key] = QVariant::fromValue<T>(value);
    }

    QnActionParameters &withArgument(const QString &key, const QVariant &value) {
        m_arguments[key] = value;

        return *this;
    }

    template<class T>
    QnActionParameters &withArgument(const QString &key, const T &value) {
        m_arguments[key] = QVariant::fromValue<T>(value);

        return *this;
    }

private:
    QVariant m_items;
    QVariantMap m_arguments;
};

#endif // QN_ACTION_PARAMETERS_H
