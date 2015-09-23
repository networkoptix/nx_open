#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/actions/action_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnActionTextFactory: public QObject, public QnWorkbenchContextAware {
public:
    /**
     * Constructor.
     * 
     * \param parent                    Context-aware parent.
     */
    QnActionTextFactory(QObject *parent);

    /**
     * Main condition checking function.
     * 
     * By default it forwards control to one of the specialized functions
     * based on the type of the default parameter. Note that these
     * specialized functions cannot access other parameters.
     * 
     * \param parameters                Parameters to check.
     * \returns                         Check result.
     */
    virtual QString text(const QnActionParameters &parameters) const;

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a resource list (<tt>Qn::ResourceType</tt>).
     */
    virtual QString text(const QnResourceList &resources) const;

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of layout items (<tt>Qn::LayoutItemType</tt>).
     */
    virtual QString text(const QnLayoutItemIndexList &layoutItems) const;

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of resource widgets. (<tt>Qn::WidgetType</tt>).
     */
    virtual QString text(const QnResourceWidgetList &widgets) const;

    /**
     * Specialized condition function that catches all action parameters that
     * are convertible to a list of workbench layouts. (<tt>Qn::LayoutType</tt>).
     */
    virtual QString text(const QnWorkbenchLayoutList &layouts) const;
};

class QnDevicesNameActionTextFactory: public QnActionTextFactory {
public:
    QnDevicesNameActionTextFactory(const QString &textTemplate, QObject *parent = NULL): 
        QnActionTextFactory(parent),
        m_template(textTemplate)
    {}
    virtual QString text(const QnResourceList &resources) const override;
private:
    const QString m_template;
};
