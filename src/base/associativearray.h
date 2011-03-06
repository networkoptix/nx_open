#ifndef cl_AssociativeArray_933
#define	cl_AssociativeArray_933

#include "base.h"

#include <math.h>

class CLValue
{
	QString value;
public:
	CLValue(){}
	CLValue(const QString& val) : value(val){}
	CLValue(const char* val) : value(val){}

	CLValue(int val) 
	{
		value.setNum(val);
	}

	CLValue(double val)
	{
		value.setNum(val);
	}

	operator bool() const { return value.toInt();}
	operator int() const { return value.toInt(); }
	operator double() const { return value.toDouble(); }
	operator QString() const { return value; }

	bool operator < ( const CLValue & other ) const
	{
		return value.toDouble() < (double)other;
	}

	bool operator > ( const CLValue & other ) const
	{
		return value.toDouble() > (double)other;
	}

	bool operator <= ( const CLValue & other ) const
	{
		return value.toDouble() <= (double)other;
	}

	bool operator >= ( const CLValue & other ) const
	{
		return value.toDouble() >= (double)other;
	}

	bool operator == ( const CLValue & other ) const
	{
		return value == other.value;
	}

	bool operator == ( int val ) const
	{
		return (fabs(value.toDouble() - val)) < 1e-6;
	}

	bool operator == ( double val ) const
	{
		return (fabs(value.toDouble() - val))< 1e-6;
	}

	bool operator == ( QString val ) const
	{
		return value == val;
	}

	bool operator != ( QString val ) const
	{
		return value != val;
	}

};

class CLAssociativeArray
{

	typedef QMap<QString, QString> MAP;
	MAP map;

public:
	CLAssociativeArray(){};
	void put(const QString& key, const CLValue& value)
	{
		map[key] = value;
	}

	bool exist(const QString& key) const
	{
		MAP::iterator it = map.find(key);
		return (it!=map.end());
	}

	CLValue get(const QString& key)
	{
		MAP::iterator it = map.find(key);
		if (it==map.end())
			return CLValue("");
		else
			return CLValue( it.value() );
	}

	CLValue get(const QString& key, const CLValue& defaultvalue)
	{
		MAP::iterator it = map.find(key);
		if (it==map.end())
			return defaultvalue;
		else
			return CLValue(it.value());
	}
};

#endif //cl_AssociativeArray_933