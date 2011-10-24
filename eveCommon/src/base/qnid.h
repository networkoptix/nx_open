#ifndef __QNID_H
#define __QNID_H

#include <QMutex>
#include <QVariant>


#define QN_EXPORT // empty define for test purpose only. move  define to .pro file

// This class is going to be exported from our common DLL. So 'QN_EXPORT' MUST be specified after 'class' identifier
class QN_EXPORT QnId
{
public:
	QnId(): m_id(0) {}
	QnId(const QString& other) { m_id = other.toInt(); }
	QnId(const QVariant& other){ m_id = other.toInt(); }
	QnId(const char* other)    { m_id = QString(other).toInt(); }
	void markUnloaded()         { if (m_id > 0) m_id = -m_id; }
	void markLoaded()           { if (m_id < 0) m_id = -m_id; }
	bool isLoaded()  const      { return m_id > 0; }
	QnId asLoaded() const      { return QnId(qAbs(m_id)); }
	bool isValid()   const      { return m_id != 0; }
	bool isSpecial() const      { return m_id >= FIRST_CUSTOM_ID; } // local generated ID. Id still unique
	uint hash()      const      { return (uint) m_id; }
	QString toString() const	{ return QString::number(m_id); }
	operator QVariant() const   { return QVariant(m_id); }
	bool operator== (const QnId& other)    const { return m_id == other.m_id; }
	bool operator!= (const QnId& other)    const { return m_id != other.m_id; }
	bool operator<  (const QnId& other)    const { return m_id < other.m_id; }
	bool operator>  (const QnId& other)    const { return m_id > other.m_id; }
	bool operator== (const QVariant& other) const { return m_id == other.toInt(); }
	bool operator!= (const QVariant& other) const { return m_id != other.toInt(); }
	static QnId generateSpecialId() {
        QMutexLocker lock(&m_mutex);
		return m_lastCustomId < INT_MAX ? ++m_lastCustomId : 0;
	}

	QnId& operator= (const QnId& other )
	{
		m_id = other.m_id;
		return *this;
	}

private:
	static const int FIRST_CUSTOM_ID = INT_MAX / 2;
	static int m_lastCustomId;
    static QMutex m_mutex;

	int m_id;

	QnId(const int value): m_id(value) {}
};
 
#endif
