#ifndef QN_ACTION_H
#define QN_ACTION_H

#include <QAction>
#include <QWeakPointer>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_globals.h>
#include "action_fwd.h"
#include "actions.h"

class QGraphicsItem;

class QnWorkbenchContext;
class QnActionCondition;
class QnActionManager;
class QnActionParameters;

/**
 * Action class that hooks into actions infrastructure to correctly check
 * conditions and provide proper targets even if it was triggered with a 
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
     * \returns                         Possible types of this action's targets.
     */
    Qn::ActionParameterTypes targetTypes() const {
        return static_cast<Qn::ActionParameterTypes>(static_cast<int>(m_flags) & Qn::TargetTypeMask);
    }

    /**
     * \returns                         Permissions that are require
     */
    Qn::Permissions requiredPermissions(const QString &target = QString()) const {
        return m_requiredPermissions.value(target);
    }

    void setRequiredPermissions(Qn::Permissions requiredPermissions);

    void setRequiredPermissions(const QString &target, Qn::Permissions requiredPermissions);

    Qn::ActionFlags flags() const {
        return m_flags;
    }

    void setFlags(Qn::ActionFlags flags);

    const QString &normalText() const {
        return m_normalText;
    }

    void setNormalText(const QString &normalText);

    const QString &toggledText() const {
        return m_toggledText;
    }

    void setToggledText(const QString &toggledText);

    const QString &pulledText() const {
        return m_pulledText;
    }

    void setPulledText(const QString &pulledText);

    QnActionCondition *condition() const {
        return m_condition.data();
    }

    void setCondition(QnActionCondition *condition);

    const QList<QnAction *> &children() const {
        return m_children;
    }

    void addChild(QnAction *action);

    void removeChild(QnAction *action);

    Qn::ActionVisibility checkCondition(Qn::ActionScopes scope, const QnActionParameters &parameters) const;

protected:
    virtual bool event(QEvent *event) override;

private slots:
    void at_toggled(bool checked);

private:
    const Qn::ActionId m_id;
    Qn::ActionFlags m_flags;
    QHash<QString, Qn::Permissions> m_requiredPermissions;
    QString m_normalText, m_toggledText, m_pulledText;
    QWeakPointer<QnActionCondition> m_condition;

    QList<QnAction *> m_children;
};

#endif // QN_ACTION_H

