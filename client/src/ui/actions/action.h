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

class QnAction: public QAction, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnAction(QnActionManager *manager, Qn::ActionId id, QObject *parent = NULL);

    virtual ~QnAction();

    Qn::ActionId id() const {
        return m_id;
    }

    Qn::ActionScopes scope() const {
        return static_cast<Qn::ActionScopes>(static_cast<int>(m_flags) & Qn::ScopeMask);
    }

    Qn::ActionTargetTypes targetTypes() const {
        return static_cast<Qn::ActionTargetTypes>(static_cast<int>(m_flags) & Qn::TargetTypeMask);
    }

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

