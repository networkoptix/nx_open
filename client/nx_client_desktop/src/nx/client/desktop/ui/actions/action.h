#pragma once

#include <QtCore/QPointer>

#include <QtWidgets/QAction>

#include <common/common_globals.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_types.h>

#include <ui/workbench/workbench_context_aware.h>
#include <client/client_globals.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_conditions.h>

class QGraphicsItem;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

/**
 * Action class that hooks into actions infrastructure to correctly check
 * conditions and provide proper action parameters even if it was triggered with a
 * hotkey.
 */
class Action: public QAction, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param id                        Identifier of this action.
     * \param parent                    Context-aware parent of this action.
     */
    Action(IDType id, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~Action();

    /**
     * \returns                         Identifier of this action.
     */
    IDType id() const;

    /**
     * \returns                         Scope of this action.
     */
    ActionScopes scope() const;

    /**
     * \returns                         Possible types of this action's default parameter.
     */
    ActionParameterTypes defaultParameterTypes() const;

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

    ClientModes mode() const;
    void setMode(ClientModes mode);

    ActionFlags flags() const;
    void setFlags(ActionFlags flags);

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
    bool hasCondition() const;

    /**
     * \param condition                 New condition for this action.
     */
    void setCondition(ConditionWrapper&& condition);

    FactoryPtr childFactory() const;
    void setChildFactory(const FactoryPtr& childFactory);

    TextFactoryPtr textFactory() const;
    void setTextFactory(const TextFactoryPtr& textFactory);

    /**
     * \returns                         Child actions. These action will appear
     *                                  in a submenu for this action.
     */
    const QList<Action *> &children() const;

    void addChild(Action *action);

    void removeChild(Action *action);

    QString toolTipFormat() const;
    void setToolTipFormat(const QString &toolTipFormat);


    /**
     * \param scope                     Scope in which action is to be executed.
     * \param parameters                Parameters for action execution.
     * \returns                         Action visibility that determines whether
     *                                  action can be executed and how it will
     *                                  appear in context menu.
     */
    ActionVisibility checkCondition(
        ActionScopes scope,
        const Parameters &parameters) const;

    void addConditionalText(ConditionWrapper&& condition, const QString &text);

    /**
     * \returns true if there is at least one conditional text
    */
    bool hasConditionalTexts() const;

    /**
     * \param parameters                Parameters for action execution.
     * \returns                         New text if condition is executed;
     *                                  empty string otherwise.
     */
    QString checkConditionalText(const Parameters &parameters) const;

protected:
    virtual bool event(QEvent *event) override;

private slots:
    void updateText();
    void updateToolTip(bool notify);
    void updateToolTipSilent();

private:
    QString defaultToolTipFormat() const;

private:
    const IDType m_id;
    ActionFlags m_flags;
    Qn::ButtonAccent m_accent{Qn::ButtonAccent::NoAccent};
    ClientModes m_mode;
    QHash<int, Qn::Permissions> m_targetPermissions;
    Qn::GlobalPermission m_globalPermission;
    QString m_normalText, m_toggledText, m_pulledText;
    QString m_toolTipFormat, m_toolTipMarker;
    ConditionWrapper m_condition;
    FactoryPtr m_childFactory;
    TextFactoryPtr m_textFactory;

    QList<Action*> m_children;

    struct ConditionalText
    {
        ConditionWrapper condition;
        QString text;
        ConditionalText() = delete;
        ConditionalText(ConditionalText&& conditionalText);
        ConditionalText(ConditionWrapper&& condition, const QString &text);
        ~ConditionalText();
    };
    std::vector<ConditionalText> m_conditionalTexts;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
