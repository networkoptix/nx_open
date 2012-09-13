#ifndef QN_ACTION_H
#define QN_ACTION_H

#include <QAction>
#include <QWeakPointer>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_globals.h>
#include <core/resource/user_resource.h>
#include "action_fwd.h"
#include "actions.h"

class QGraphicsItem;

class QnWorkbenchContext;
class QnActionCondition;
class QnActionManager;
class QnActionParameters;

/**
 * Action class that hooks into actions infrastructure to correctly check
 * conditions and provide proper action parameters even if it was triggered with a 
 * hotkey.
 */
class QnAction: public QAction, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param id                        Identifier of this action.
     * \param parent                    Context-aware parent of this action.
     */
    QnAction(Qn::ActionId id, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnAction();

    /**
     * \returns                         Identifier of this action.
     */
    Qn::ActionId id() const {
        return m_id;
    }

    /**
     * \returns                         Scope of this action.
     */
    Qn::ActionScopes scope() const {
        return static_cast<Qn::ActionScopes>(static_cast<int>(m_flags) & Qn::ScopeMask);
    }

    /**
     * \returns                         Possible types of this action's default parameter.
     */
    Qn::ActionParameterTypes defaultParameterTypes() const {
        return static_cast<Qn::ActionParameterTypes>(static_cast<int>(m_flags) & Qn::TargetTypeMask);
    }

    /**
     * \param target                    Name of the action parameter.
     * \returns                         Permissions that are required for the provided parameter.
     */
    Qn::Permissions requiredPermissions(const QString &target = QString()) const {
        return m_permissions.value(target).required;
    }

    void setRequiredPermissions(Qn::Permissions requiredPermissions);

    /**
     * \param target                    Name of action parameter.
     * \param requiredPermissions       Permissions required for the provided parameter.
     */
    void setRequiredPermissions(const QString &target, Qn::Permissions requiredPermissions);

    /**
     * \param target                    Name of the action parameter.
     * \returns                         Permissions that must not be present for the provided parameter.
     */
    Qn::Permissions forbiddenPermissions(const QString &target = QString()) const {
        return m_permissions.value(target).forbidden;
    }

    void setForbiddenPermissions(const QString &target, Qn::Permissions forbiddenPermissions);

    void setForbiddenPermissions(Qn::Permissions forbiddenPermissions);

    Qn::ActionFlags flags() const {
        return m_flags;
    }

    void setFlags(Qn::ActionFlags flags);

    /**
     * \returns                         Default text of this action.
     */
    const QString &normalText() const {
        return m_normalText;
    }

    /**
     * \param normalText                Default text of this action.
     */
    void setNormalText(const QString &normalText);

    /**
     * \returns                         Text for this action that is to be used when it is toggled.
     */
    const QString &toggledText() const {
        return m_toggledText.isEmpty() ? m_normalText : m_toggledText;
    }

    /**
     * \param toggledText               Text for this action that is to be used when it is toggled.
     *                                  If empty, default text will be used.
     */
    void setToggledText(const QString &toggledText);

    /**
     * \returns                         Text for this action that is to be used when it is pulled into 
     *                                  the enclosing menu.
     */
    const QString &pulledText() const {
        return m_pulledText.isEmpty() ? m_normalText : m_pulledText;
    }

    /**
     * \param pulledText                Text for this action that is to be used when it is pulled into the
     *                                  enclosing menu. If empty, default text will be used.
     */
    void setPulledText(const QString &pulledText);

    /**
     * \returns                         Condition associated with this action, of NULL if none.
     */
    QnActionCondition *condition() const {
        return m_condition.data();
    }

    /**
     * \param condition                 New condition for this action.
     */
    void setCondition(QnActionCondition *condition);

    /**
     * \returns                         Child actions. These action will appear
     *                                  in a submenu for this action.
     */
    const QList<QnAction *> &children() const {
        return m_children;
    }

    void addChild(QnAction *action);

    void removeChild(QnAction *action);

    /**
     * \param scope                     Scope in which action is to be executed.
     * \param parameters                Parameters for action execution.
     * \returns                         Action visibility that determines whether
     *                                  action can be executed and how it will
     *                                  appear in context menu.
     */
    Qn::ActionVisibility checkCondition(Qn::ActionScopes scope, const QnActionParameters &parameters) const;

    void addConditionalText(QnActionCondition *condition, const QString &text);

    /**
     * \returns true if there is at least one conditional text
    */
    bool hasConditionalTexts();

    /**
     * \param parameters                Parameters for action execution.
     * \returns                         New text if condition is executed;
     *                                  empty string otherwise.
     */
    QString checkConditionalText(const QnActionParameters &parameters) const;

protected:
    virtual bool event(QEvent *event) override;

private slots:
    void updateText();

private:
    struct Permissions {
        Permissions(): required(0), forbidden(0) {}

        Qn::Permissions required;
        Qn::Permissions forbidden;
    };

    const Qn::ActionId m_id;
    Qn::ActionFlags m_flags;
    QHash<QString, Permissions> m_permissions;
    QString m_normalText, m_toggledText, m_pulledText;
    QWeakPointer<QnActionCondition> m_condition;

    QList<QnAction *> m_children;
    QHash<QnActionCondition *, QString> m_textConditions;
};

#endif // QN_ACTION_H

