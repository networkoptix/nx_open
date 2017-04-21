#pragma once

#include <QtCore/QVariant>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/ui/actions/action_parameter_types.h>

#include "actions.h"

class QGraphicsItem;

/**
 * Parameter pack for an action.
 *
 * It always contains a 'default' parameter, accessible via <tt>items()</tt>
 * or <tt>argument(-1)</tt>. This parameter is expected to be of one of
 * the types convertible to <tt>Qn::ResourceType</tt>. It is normally passed
 * as the first argument to the <tt>QnActionParameters</tt>'s constructor.
 */
class QnActionParameters {
public:
    typedef QHash<int, QVariant> ArgumentHash;

    QnActionParameters(const ArgumentHash &arguments = ArgumentHash());

    explicit QnActionParameters(const QVariant &items, const ArgumentHash &arguments = ArgumentHash());

    template<class Resource>
    QnActionParameters(const QnSharedResourcePointer<Resource> &resource, const ArgumentHash &arguments = ArgumentHash()) {
        setArguments(arguments);
        setItems(QVariant::fromValue<QnResourcePtr>(resource));
    }

    template<class Resource>
    QnActionParameters(const QnSharedResourcePointerList<Resource> &resources, const ArgumentHash &arguments = ArgumentHash()) {
        setArguments(arguments);
        setItems(QVariant::fromValue<QnResourceList>(resources));
    }

    QnActionParameters(const QList<QGraphicsItem *> &items, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(QnResourceWidget *widget, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(const QnResourceWidgetList &widgets, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(const QnWorkbenchLayoutList &layouts, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(const QnLayoutItemIndexList &layoutItems, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(const QnVideoWallItemIndexList &videoWallItems, const ArgumentHash &arguments = ArgumentHash());

    QnActionParameters(const QnVideoWallMatrixIndexList &videoWallMatrices, const ArgumentHash &arguments = ArgumentHash());

    QVariant items() const {
        return argument(-1);
    }

    void setItems(const QVariant &items);

    void setResources(const QnResourceList &resources);

    nx::client::desktop::ui::action::ActionParameterType type(int key = -1) const;

    int size(int key = -1) const;

    QnResourceList resources(int key = -1) const;

    QnResourcePtr resource(int key = -1) const;

    QnLayoutItemIndexList layoutItems(int key = -1) const;

    QnVideoWallItemIndexList videoWallItems(int key = -1) const;

    QnVideoWallMatrixIndexList videoWallMatrices(int key = -1) const;

    QnWorkbenchLayoutList layouts(int key = -1) const;

    QnResourceWidget *widget(int key = -1) const;

    template<class WidgetClass>
    WidgetClass *widget(int key = -1) const {
        return dynamic_cast<WidgetClass *>(widget(key));
    }

    QnResourceWidgetList widgets(int key = -1) const;

    const ArgumentHash &arguments() const {
        return m_arguments;
    }

    QVariant argument(int key, const QVariant &defaultValue = QVariant()) const {
        return m_arguments.value(key, defaultValue);
    }


    template<class T>
    T argument(int key, const T &defaultValue = T()) const {
        ArgumentHash::const_iterator pos = m_arguments.find(key);
        if(pos == m_arguments.end())
            return defaultValue;

        QVariant result = *pos;
        if(!result.convert(static_cast<QVariant::Type>(qMetaTypeId<T>())))
            return defaultValue;

        return result.value<T>();
    }

    bool hasArgument(int key) const {
        return m_arguments.contains(key);
    }

    void setArguments(const ArgumentHash &arguments);

    void setArgument(int key, const QVariant &value);

    template<class T>
    void setArgument(int key, const T &value) {
        setArgument(key, QVariant::fromValue<T>(value));
    }

    QnActionParameters &withArgument(int key, const QVariant &value) {
        setArgument(key, value);

        return *this;
    }

    template<class T>
    QnActionParameters &withArgument(int key, const T &value) {
        setArgument(key, value);

        return *this;
    }

    nx::client::desktop::ui::action::ActionScopes scope() const;
    void setScope(nx::client::desktop::ui::action::ActionScopes scope);

private:
    void init(const QVariant &items, const ArgumentHash &arguments);

private:
    ArgumentHash m_arguments;
    nx::client::desktop::ui::action::ActionScopes m_scope;
};

Q_DECLARE_METATYPE(QnActionParameters)
