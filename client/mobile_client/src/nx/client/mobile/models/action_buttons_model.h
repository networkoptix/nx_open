#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/uuid.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {

namespace core {
class TwoWayAudioAvailabilityWatcher;
} // core

namespace mobile {

class PtzAvailabilityWatcher;

class ActionButtonsModel: public Connective<QAbstractListModel>
{
    Q_OBJECT
    using base_type = Connective<QAbstractListModel>;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)

public:
    enum ButtonType
    {
        PtzButton,
        TwoWayAudioButton,
        SoftTriggerButton
    };

    Q_ENUM(ButtonType)

public:
    ActionButtonsModel(QObject* parent = nullptr);
    virtual ~ActionButtonsModel();

    void setResourceId(const QString& resourceId);
    QString resourceId() const;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void resourceIdChanged();

private:
    struct Button;
    struct SoftwareButton;
    using ButtonPtr = QSharedPointer<Button>;
    using ButtonList = QList<ButtonPtr>;

private:
    void handleResourceIdChanged();

    int softTriggerButtonStartIndex() const;

    void removeSoftTriggerButtons();

    void updateSoftTriggerButtons();

    void updatePtzButtonVisibility();

    void updateTwoWayAudioButtonVisibility();

    void insertButton(int index, const ButtonPtr& button);

    void removeButton(int index);

    bool ptzButtonVisible() const;

    ButtonList::const_iterator lowerBoundByRuleId(const QnUuid& ruleId) const;

    int buttonIndexByRuleId(const QnUuid& ruleId) const;

    int insertionIndexByRuleId(const QnUuid& ruleId) const;

    static QnUuid getRuleId(const ButtonPtr& button);

private:
    QnUuid m_resourceId;
    ButtonList m_buttons;
    QScopedPointer<PtzAvailabilityWatcher> m_ptzAvailabilityWatcher;
    QScopedPointer<core::TwoWayAudioAvailabilityWatcher> m_twoWayAudioAvailabilityWatcher;
};

} // namespace mobile
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::mobile::ActionButtonsModel::ButtonType)
