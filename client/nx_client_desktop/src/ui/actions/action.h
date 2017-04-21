#pragma once

#include <QtCore/QPointer>

#include <QtWidgets/QAction>

#include <common/common_globals.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_types.h>

#include <ui/workbench/workbench_context_aware.h>
#include <client/client_globals.h>

#include "actions.h"

class QGraphicsItem;

class QnWorkbenchContext;

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
    nx::client::desktop::ui::action::ActionScopes scope() const;

    /**
     * \returns                         Possible types of this action's default parameter.
     */
    nx::client::desktop::ui::action::ActionParameterTypes defaultParameterTypes() const;

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
    void setRequiredGlobalPermission(Qn::GlobalPermission requiredPermissions);

    nx::client::desktop::ui::action::ClientModes mode() const;
    void setMode(nx::client::desktop::ui::action::ClientModes mode);

    nx::client::desktop::ui::action::ActionFlags flags() const;
    void setFlags(nx::client::desktop::ui::action::ActionFlags flags);

    Qn::ButtonAccent accent() const;
    void setAccent(Qn::ButtonAccent value);

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
    nx::client::desktop::ui::action::ConditionPtr condition() const;

    /**
     * \param condition                 New condition for this action.
     */
    void setCondition(const nx::client::desktop::ui::action::ConditionPtr& condition);

    nx::client::desktop::ui::action::FactoryPtr childFactory() const;
    void setChildFactory(const nx::client::desktop::ui::action::FactoryPtr& childFactory);

    nx::client::desktop::ui::action::TextFactoryPtr textFactory() const;
    void setTextFactory(const nx::client::desktop::ui::action::TextFactoryPtr& textFactory);

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
    nx::client::desktop::ui::action::ActionVisibility checkCondition(
        nx::client::desktop::ui::action::ActionScopes scope,
        const QnActionParameters &parameters) const;

    void addConditionalText(nx::client::desktop::ui::action::ConditionPtr condition, const QString &text);

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
    nx::client::desktop::ui::action::ActionFlags m_flags;
    Qn::ButtonAccent m_accent{Qn::ButtonAccent::NoAccent};
    nx::client::desktop::ui::action::ClientModes m_mode;
    QHash<int, Qn::Permissions> m_targetPermissions;
    Qn::GlobalPermission m_globalPermission;
    QString m_normalText, m_toggledText, m_pulledText;
    QString m_toolTipFormat, m_toolTipMarker;
    nx::client::desktop::ui::action::ConditionPtr m_condition;
    nx::client::desktop::ui::action::FactoryPtr m_childFactory;
    nx::client::desktop::ui::action::TextFactoryPtr m_textFactory;

    QList<QnAction *> m_children;

    struct ConditionalText
    {
        nx::client::desktop::ui::action::ConditionPtr condition;
        QString text;
        ConditionalText(){}
        ConditionalText(nx::client::desktop::ui::action::ConditionPtr condition, const QString &text):
            condition(condition), text(text) {}
    };
    QList<ConditionalText> m_conditionalTexts;
};

Q_DECLARE_METATYPE(QnAction *)

