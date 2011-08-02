#ifndef mac_address_h_2244
#define mac_address_h_2244

class QnMacAddress
{
public:
    QnMacAddress();
    QnMacAddress(unsigned char* mac);
    QnMacAddress(const QString& mac);
    ~QnMacAddress();

    bool isEmpty() const;

    QString toString() const;
    const unsigned char* toBytes() const;

    void setByte(int number, unsigned char val);
    unsigned char getByte(int number) const;

    QnMacAddress& operator = ( const QnMacAddress& other );
    bool operator == ( const QnMacAddress& other ) const;
    bool operator != ( const QnMacAddress& other ) const;
    bool operator < ( const QnMacAddress& other ) const;
    bool operator <= ( const QnMacAddress& other ) const;
    bool operator > ( const QnMacAddress& other ) const;
    bool operator >= ( const QnMacAddress& other ) const;

private:
    unsigned char m_data[6];


};



#endif //mac_address_h_2244