#ifndef aligned_data_h_1636
#define aligned_data_h_1636

#include "log.h"

class CLAlignedData
{
public:
	explicit CLAlignedData(int alignment, unsigned long size):
	m_alignment(alignment)
	{
		init(alignment, size, m_all_data, m_aligned_data, m_capacity);

	}

	~CLAlignedData()
	{
		delete[] m_all_data;
	}
	
	
	char* data()
	{
		return m_aligned_data;
	}
	


	

	unsigned long capacity() const
	{
		return m_capacity;
	}


	void change_capacity(unsigned long new_capacity)
	{

		char* all_data;
		char* aligned_data;
		unsigned long capacity;
		
		init(m_alignment, new_capacity, all_data, aligned_data, capacity);

		memcpy(aligned_data, m_aligned_data, m_capacity); // copy old data 

		delete[] m_all_data; // delete old data

		m_all_data = all_data;
		m_aligned_data = aligned_data;
		m_capacity = capacity;

		cl_log.log("CLAlignedData::change_capacity is called", cl_logWARNING);
		
	}


private:
	CLAlignedData(const CLAlignedData&){};
	CLAlignedData& operator=(const CLAlignedData&){return *this;}

	void init(int alignment, unsigned long size, char*& all_data, char*& aligned_data, unsigned long& capacity)
	{
		all_data = new char[alignment + size];
		aligned_data = all_data;

		capacity = alignment + size;

		unsigned long mod = ((unsigned long)all_data)%alignment;

		if (mod)
		{
			aligned_data += alignment - mod;
			capacity-=(alignment - mod);
		}
	}

private:

	char* m_all_data;
	char* m_aligned_data;
	int m_alignment;
	unsigned long m_capacity;

};

#endif //aligned_data_h_1636