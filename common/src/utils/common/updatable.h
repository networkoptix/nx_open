#pragma once

class QnUpdatable {
public:
    QnUpdatable();
    virtual ~QnUpdatable();

    void beginUpdate();
    void endUpdate();

protected:
    bool isUpdating() const;

    /** Called in each beginUpdate(). */
    virtual void beginUpdateInternal();

    /** Called in each endUpdate(). */
    virtual void endUpdateInternal();

    /** Called in first beginUpdate(). */
    virtual void beforeUpdate();

    /** Called in last endUpdate(). */
    virtual void afterUpdate();
private:
    int m_updateCount;
};


template<typename Updatable>
class QnUpdatableGuard {
public:
    QnUpdatableGuard(Updatable* source): m_guard(source)
    {
        if (m_guard)
            m_guard->beginUpdate();
    }

    virtual ~QnUpdatableGuard()
    {
        if (m_guard)
            m_guard->endUpdate();
    }

private:
    QPointer<Updatable> m_guard;
};