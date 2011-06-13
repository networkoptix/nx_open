#include "bytearray.h"
#include <assert.h>
#include <memory.h>
#include "log.h"

CLByteArray::CLByteArray(unsigned int alignment, unsigned int capacity):
m_alignment(alignment),
m_size(0)
{

	capacity+=alignment;

	m_data = new char[capacity];

	int shift  = (alignment==0) ? 0 : (alignment - ((unsigned long)m_data)%alignment);

	m_al_data = m_data + shift;

	m_capacity = capacity - shift;

	m_ignore = 0;
}

CLByteArray::~CLByteArray()
{
	delete[] m_data;
}

const char* CLByteArray::data() const
{
	return m_al_data + m_ignore;
}

char *CLByteArray::data()
{
	return m_al_data + m_ignore;
}

void CLByteArray::ignore_first_bytes(int bytes_to_ignore)
{
	m_ignore = bytes_to_ignore;
}

unsigned int CLByteArray::size() const
{
	return m_size - m_ignore;
}

unsigned int CLByteArray::capacity() const
{
	return m_capacity;
}

bool CLByteArray::increase_capacity(unsigned int new_capacity)
{
	cl_log.log("CLByteArray::increase_capacity:reallocate", cl_logWARNING);

	new_capacity += m_alignment;

	if (new_capacity <= m_size + m_alignment)
		return false;

	char* new_data = new char[new_capacity]; 
	if (!new_data)
		return false;

	int shift = (m_alignment==0) ? 0 : (m_alignment - ((unsigned long)new_data)%m_alignment);
	m_capacity = new_capacity - shift;

	memcpy(new_data + shift, m_al_data, m_size); // copy old data to new

	delete[] m_data; // delete old data

	m_data = new_data;
	m_al_data = m_data + shift;

	// size remains the same

	return true;

}

unsigned int CLByteArray::write(const char* data, unsigned int size)
{
	if (size > m_capacity - m_size) // if we do not have anougth space 
	{
		if (!increase_capacity(m_capacity*2 + size))
			return 0;
	}

	memcpy(m_al_data + m_size, data, size);
	m_size += size;
	return size;
}

unsigned int CLByteArray::write(const QByteArray& data)
{
    return write(data.constData(), data.size());
}

unsigned int CLByteArray::write(const char* data, unsigned int size, int abs_shift  )
{
	if (size + abs_shift > m_capacity) // if we do not have anougth space 
	{
		if (!increase_capacity(m_capacity*2 + size))
			return 0;
	}

	memcpy(m_al_data + abs_shift, data, size);

	if (size + abs_shift > m_size)
		m_size = size + abs_shift;

	return size;

}

unsigned int CLByteArray::write(const CLByteArray& other )
{
	return write(other.data(), other.size());
}

void CLByteArray::clear()
{
	m_size = 0;
}

char* CLByteArray::prepareToWrite(int size)
{
	if (size > m_capacity - m_size) // if we do not have anogth space 
	{
		if (!increase_capacity(m_capacity*2 + size))
			return 0;
	}

	return m_al_data + m_size;
}

void CLByteArray::done(int size)
{
	m_size+=size;
}

void CLByteArray::removeZerrowsAtTheEnd()
{
	while(1)
	{
		if (m_size>0 && m_al_data[m_size-1]==0)
			m_size--;
		else
			break;
	}

}
