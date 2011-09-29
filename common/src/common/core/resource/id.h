#ifndef id_h_2344
#define id_h_2344

class QnId
{
public:
    QnId();
    inline QnId(const QString &id) : m_id(id) {}
    inline ~QnId() {}

    inline QnId &operator=(const QnId &other)
    {
        m_id = other.m_id;
        return *this;
    }

    inline void setEmpty()
    { m_id.clear(); }

    inline QString toString() const
    { return m_id; }

    inline operator QString() const
    { return m_id; }

    inline bool operator==(const QnId &other) const
    { return m_id == other.m_id; }
    inline bool operator!=(const QnId &other) const
    { return !operator==(other); }

    inline bool operator<(const QnId &other) const
    { return m_id < other.m_id; }
    inline bool operator<=(const QnId &other) const
    { return m_id <= other.m_id; }

    inline bool operator>(const QnId &other) const
    { return m_id > other.m_id; }
    inline bool operator>=(const QnId &other) const
    { return m_id >= other.m_id; }

private:
    QString m_id;
};

#endif //resource_id_h_2344
