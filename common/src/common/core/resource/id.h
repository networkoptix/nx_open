#ifndef id_h_2344
#define id_h_2344

class QnId
{
public:
    QnId()
    {
        m_id = generateNewId();
    }

    QnId(const QString& id)
    {
        m_id = id;
    }

    ~QnId(){}

    QnId& operator = ( const QnId& other )
    {
        m_id = other.m_id;
        return *this;
    }

    bool operator == ( const QnId& other ) const
    {
        return (m_id == other.m_id);
    }

    bool operator != ( const QnId& other ) const
    {
        return (m_id != other.m_id);
    }

    bool operator < ( const QnId& other ) const
    {
        return (m_id < other.m_id);
    }

    bool operator <= ( const QnId& other ) const
    {
        return (m_id <= other.m_id);
    }

    bool operator > ( const QnId& other ) const
    {
        return (m_id > other.m_id);
    }

    bool operator >= ( const QnId& other ) const
    {
        return (m_id >= other.m_id);
    }

    void setEmpty()
    {
        m_id = "";
    }


    QString toString() const
    {
        return m_id;
    }

    

private:
    QString generateNewId();
private:
    QString m_id;
   
};


#endif //resource_id_h_2344