#pragma once

#include <network/system_description.h>

class QnCloudSystemDescription: public QnSystemDescription
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
        bool running);

    virtual ~QnCloudSystemDescription() = default;

    void setRunning(bool running);

public: // Overrides
    virtual bool isCloudSystem() const override;

    virtual bool isRunning() const override;

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
        bool running);

private:
    const QString m_ownerEmail;
    const QString m_ownerFullName;
    bool m_running;
};