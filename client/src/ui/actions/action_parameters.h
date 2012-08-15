#ifndef QN_ACTION_PARAMETERS_H
#define QN_ACTION_PARAMETERS_H

#include <QtCore/QVariant>

#include <utils/common/mpl.h>
#include <core/resource/resource_fwd.h>

#include "action_fwd.h"
#include "actions.h"

class QGraphicsItem;

/**
 * Parameter pack for an action.
 * 
 * It always contains a 'default' parameter, accessible via <tt>items()</tt>
 * or <tt>argument(QString())</tt>. This parameter is expected to be of one of
 * the types convertible to <tt>Qn::ResourceType</tt>. It is normally passed
 * as the first argument to the <tt>QnActionParameters</tt>'s constructor.
 */
class QnActionParameters {
public:
    QnActionParameters(const QVariantMap &arguments = QVariantMap());

    explicit QnActionParameters(const QVariant &items, const QVariantMap &arguments = QVariantMap());

    template<class Resource>
    QnActionParameters(const QnSharedResourcePointer<Resource> &resource, const QVariantMap &arguments = QVariantMap()) {
        setArguments(arguments);
        setItems(QVariant::fromValue<QnResourcePtr>(resource));
    }

    template<class Resource>
    QnActionParameters(const QnSharedResourcePointerList<Resource> &resources, const QVariantMap &arguments = QVariantMap()) {
        setArguments(arguments);
        setItems(QVariant::fromValue<QnResourceList>(resources));
    }

    QnActionParameters(const QList<QGraphicsItem *> &items, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(QnResourceWidget *widget, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnResourceWidgetList &widgets, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnWorkbenchLayoutList &layouts, const QVariantMap &arguments = QVariantMap());

    QnActionParameters(const QnLayoutItemIndexList &layoutItems, const QVariantMap &arguments = QVariantMap());

    QVariant items() const {
        return argument(QString());
    }

    void setItems(const QVariant &items);

    Qn::ActionParameterType type(const QString &key = QString()) const;

    int size(const QString &key = QString()) const;
    
    QnResourceList resources(const QString &key = QString()) const;

    QnResourcePtr resource(const QString &key = QString()) const;

    QnLayoutItemIndexList layoutItems(const QString &key = QString()) const;

    QnWorkbenchLayoutList layouts(const QString &key = QString()) const;

    QnResourceWidget *widget(const QString &key = QString()) const;

    QnResourceWidgetList widgets(const QString &key = QString()) const;

    const QVariantMap &arguments() const {
        return m_arguments;
    }

    QVariant argument(const QString &key) const {
        return m_arguments.value(key);
    }

    template<class T>
    T argument(const QString &key) const {
        return argument(key).value<T>();
    }

    bool hasArgument(const QString &key) const {
        return m_arguments.contains(key);
    }

    void setArguments(const QVariantMap &arguments);

    void setArgument(const QString &key, const QVariant &value);

    template<class T>
    void setArgument(const QString &key, const T &value) {
        setArgument(key, QVariant::fromValue<T>(value));
    }

    QnActionParameters &withArgument(const QString &key, const QVariant &value) {
        setArgument(key, value);

        return *this;
    }

    template<class T>
    QnActionParameters &withArgument(const QString &key, const T &value) {
        setArgument(key, value);

        return *this;
    }

private:
    QVariantMap m_arguments;
};

#endif // QN_ACTION_PARAMETERS_H
