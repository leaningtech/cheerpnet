#ifndef __UTIL__

namespace [[cheerp::genericjs]] utils
{

template<class T>
class Vector
{
protected:
	T* data;
	uint32_t capacity;
	uint32_t length;
public:
	Vector():data(nullptr),capacity(0),length(0)
	{
	}
	~Vector()
	{
		if(data)
			free(data);
	}
	bool isEmpty() const
	{
		return length == 0;
	}
	uint32_t size() const
	{
		return length;
	}
	T* pushBack(const T& v)
	{
		if(length == capacity)
		{
			if(data == nullptr)
			{
				capacity = 2;
				data = (T*)malloc(capacity*sizeof(T));
			}
			else
			{
				capacity = capacity * 3 / 2;
				data = (T*)realloc(data, capacity*sizeof(T));
			}
		}
		assert(capacity > length);
		int idx = length++;
		data[idx] = v;
		return &data[idx];
	}
	T popBack()
	{
		assert(length > 0);
		T ret = data[length-1];
		length--;
		return ret;
	}
	void erase(T* p)
	{
		int idx = p - data;
		length--;
		for(int i=idx;i<length;i++)
		{
			data[i] = data[i+1];
		}
	}
	T& operator[](int idx)
	{
		assert(idx < length);
		return data[idx];
	}
};

}

#endif // __UTIL__