
#pragma once

#include <network/system_description.h>

class QnCloudSystemDescription : public QnSystemDescription
{
    Q_OBJECT
    using base_type = QnSystemDescription;

public:
    typedef QSharedPointer<QnCloudSystemDescription> PointerType;

    static PointerType create(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName,
        const QString& ownerEmail,
        const QString& ownerFullName,
        bool online);

    virtual ~QnCloudSystemDescription() = default;

    void setOnlineState(bool online);

public: // Overrides
    virtual bool isCloudSystem() const;

    virtual bool isOnline() const override;

    virtual bool isNewSystem() const override;

    virtual QString ownerAccountEmail() const override;

    virtual QString ownerFullName() const override;

private:
    QnCloudSystemDescription(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName,
        const QString& ownerEmail,
        const QString& ownerFullName,
        bool online);

private:
    const QString m_ownerEmail;
    const QString m_ownerFullName;
    bool m_online;
};