#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QnAbstractSaveStateManager: public Connective<QObject>
{
    Q_OBJECT
    Q_FLAGS(SaveStateFlags)

    using base_type = Connective<QObject>;

public:
    enum SaveStateFlag
    {
        IsBeingSaved = 0x1, //< Is currently being saved to the server.
        IsChanged = 0x2,    //< Unsaved changes are present.
    };
    Q_DECLARE_FLAGS(SaveStateFlags, SaveStateFlag)

    QnAbstractSaveStateManager(QObject* parent = nullptr);

    SaveStateFlags flags(const QnUuid& id) const;
    void setFlags(const QnUuid& id, SaveStateFlags flags);

    void clean(const QnUuid& id);

    bool isBeingSaved(const QnUuid& id) const;
    bool isChanged(const QnUuid& id) const;

    /**
     * Check if item is changed and is not being saved right now.
     */
    bool isSaveable(const QnUuid& id) const;

    void markBeingSaved(const QnUuid& id, bool saved);
    void markChanged(const QnUuid& id, bool changed);

signals:
    void flagsChanged(const QnUuid& id, SaveStateFlags flags);

private:
    mutable QnMutex m_mutex;
    QHash<QnUuid, SaveStateFlags> m_flags;
};