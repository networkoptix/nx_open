#pragma once

#include <QtWidgets/QAction>

#include <ui/workbench/workbench_context_aware.h>
#include <client/client_globals.h>

#include "action_conditions.h"
#include "action_factories.h"
#include "action_text_factories.h"
#include "action_fwd.h"
#include "actions.h"

class QGraphicsItem;

class QnWorkbenchContext;
class QnActionCondition;
class QnActionFactory;
class QnActionManager;
class QnActionParameters;

/**
 * Action class that hooks into actions infrastructure to correctly check
 * conditions and provide proper action parameters even if it was triggered with a
 * hotkey.
 */
class QnAction: public QAction, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param id                        Identifier of this action.
     * \param parent                    Context-aware parent of this action.
     */
    QnAction(QnActions::IDType id, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnAction();

    /**
     * \returns                         Identifier of this action.
     */
    QnActions::IDType id() const;

    /**
     * \returns                         Scope of this action.
     */
    Qn::ActionScopes scope() const;

    /**
     * \returns                         Possible types of this action's default parameter.
     */
    Qn::ActionParameterTypes defaultParameterTypes() const;

    /**
     * \param target                    Action parameter key.
     * \returns                         Permissions that are required for the provided parameter.
     */
    Qn::Permissions requiredTargetPermissions(int target = -1) const;

    /**
    * \param target                    Action parameter key.
    * \param requiredPermissions       Permissions required for the provided parameter.
    */
    void setRequiredTargetPermissions(int target, Qn::Permissions requiredPermissions);

    /**
    * \param requiredPermissions       Permissions required for the default target.
    */
    void setRequiredTargetPermissions(Qn::Permissions requiredPermissions);

    /**
    * \param requiredPermissions       Global permissions that the current user must have.
    */
    void setRequiredGlobalPermissions(Qn::GlobalPermissions requiredPermissions);


    QnActionTypes::ClientModes mode() const;

    void setMode(QnActionTypes::ClientModes mode);

    Qn::ActionFlags flags() const;

    void setFlags(Qn::ActionFlags flags);

    /**
     * \returns                         Default text of this action.
     */
    const QString &normalText() const;

    /**
     * \param normalText                Default text of this action.
     */
    void setNormalText(const QString &normalText);

    /**
     * \returns                         Text for this action that is to be used when it is toggled.
     */
    const QString &toggledText() const;

    /**
     * \param toggledText               Text for this action that is to be used when it is toggled.
     *                                  If empty, default text will be used.
     */
    void setToggledText(const QString &toggledText);

    /**
     * \returns                         Text for this action that is to be used when it is pulled into
     *                                  the enclosing menu.
     */
    const QString &pulledText() const;

    /**
     * \param pulledText                Text for this action that is to be used when it is pulled into the
     *                                  enclosing menu. If empty, default text will be used.
     */
    void setPulledText(const QString &pulledText);

    /**
     * \returns                         Condition associated with this action, of NULL if none.
     */
    QnActionCondition *condition() const;

    /**
     * \param condition                 New condition for this action.
     */
    void setCondition(QnActionCondition *condition);

    QnActionFactory *childFactory() const;

    void setChildFactory(QnActionFactory *childFactory);

    QnActionTextFactory *textFactory() const;

    void setTextFactory(QnActionTextFactory *textFactory);

    /**
     * \returns                         Child actions. These action will appear
     *                                  in a submenu for this action.
     */
    const QList<QnAction *> &children() const;

    void addChild(QnAction *action);

    void removeChild(QnAction *action);

    QString toolTipFormat() const;
    void setToolTipFormat(const QString &toolTipFormat);


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
    void updateToolTip(bool notify);
    void updateToolTipSilent();

private:
    QString defaultToolTipFormat() const;

private:
    const QnActions::IDType m_id;
    Qn::ActionFlags m_flags;
    QnActionTypes::ClientModes m_mode;
    QHash<int, Qn::Permissions> m_targetPermissions;
    Qn::GlobalPermissions m_globalPermissions;
    QString m_normalText, m_toggledText, m_pulledText;
    QString m_toolTipFormat, m_toolTipMarker;
    QPointer<QnActionCondition> m_condition;
    QPointer<QnActionFactory> m_childFactory;
    QPointer<QnActionTextFactory> m_textFactory;

    QList<QnAction *> m_children;

    struct ConditionalText
    {
        QnActionCondition * condition;
        QString text;
        ConditionalText(){}
        ConditionalText(QnActionCondition * condition, const QString &text):
            condition(condition), text(text) {}
    };
    QList<ConditionalText> m_conditionalTexts;
};

Q_DECLARE_METATYPE(QnAction *)

