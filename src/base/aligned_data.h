#ifndef aligned_data_h_1636
#define aligned_data_h_1636

class CLAlignedData
{
public:
	explicit CLAlignedData(int alignment, int size)
	{
		m_all_data = new char[alignment + size];
		m_aligned_data = m_all_data;

		unsigned long mod = ((unsigned long)m_all_data)%alignment;
		
		if (mod)
			m_aligned_data += alignment - mod;

	}

	~CLAlignedData()
	{
		delete[] m_all_data;
	}
	
	operator char*()
	{
		return m_aligned_data;
	}

	operator const char*() const
	{
		return m_aligned_data;
	}

	operator unsigned char*()
	{
		return (unsigned char*)m_aligned_data;
	}

	operator const unsigned char*() const
	{
		return (unsigned char*)m_aligned_data;
	}


private:
	CLAlignedData(const CLAlignedData&){};
	CLAlignedData& operator=(const CLAlignedData&){return *this;}
private:

	char* m_all_data;
	char* m_aligned_data;

};

#endif //aligned_data_h_1636