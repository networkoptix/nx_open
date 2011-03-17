#ifndef CL_bytearray_1207
#define CL_bytearray_1207

// the purpose of this class is to provide array container for alignment data
class CLByteArray
{
public:

	// creates array with alignment and capacity
	// note that capacity() function might return slightly different value from the one passed to constructor 
	explicit CLByteArray(unsigned int alignment, unsigned int capacity);

	~CLByteArray();

	// returns const pointer to alignment data
	const char *data() const; 

	// returns pointer to alignment data
	char *data();

	// return actual data size
	unsigned int size() const;

	unsigned int capacity() const;

	//writes size bytes to array starting from current position.
	// if size > (capacity - curr_size) then capacity automatically increased by factor of 2
	unsigned int write(const char* data, unsigned int size );

	//writes size bytes to array starting from abs_shift position.
	// if size + shift > (capacity) then capacity automatically increased by factor of 2
	unsigned int write(const char* data, unsigned int size, int abs_shift );

	// writes other array to current position
	// if other.size > (this.capacity - this.curr_size) then capacity automatically increased by factor of 2
	unsigned int write(const CLByteArray& other );

	// sets current write position and size to 0;
	// capacity remains the same
	void clear();

	// this function makes sure that array has capacity to write size bytes 
	// returns a pointer
	// this function is used with done()
	char* prepareToWrite(int size);

	void done(int size);

	void ignore_first_bytes(int bytes_to_ignore);

	//======

	//av cam
	void removeZerrowsAtTheEnd();

private:
	CLByteArray(const CLByteArray&){};
	CLByteArray& operator=(const CLByteArray&){};

	// this function realocates data and changes capacity.
	// if new capacity smaller than current size+alignment this function does nothing 
	// return false if can not allocate new memorry
	bool increase_capacity(unsigned int new_capacity);

private:
	unsigned int m_alignment;

	unsigned int m_size;
	unsigned int m_capacity;

	char* m_data;

	char* m_al_data;

	int m_ignore;

};

#endif //CL_bytearray_1207