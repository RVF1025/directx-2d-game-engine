#pragma once
#include <list>

template<class T, int SIZE = 100>
class ObjectPool
{
public:
	ObjectPool() {
		for (int i = 0; i < SIZE; ++i) {
			T* newObject = new T();
			mObjects.push_back(newObject);
		}
	}
	~ObjectPool()
	{
		while (!mObjects.empty()) {
			mObjects.clear();
		}
	}
	T* PopObject()
	{
		if (!mObjects.empty())
		{
			T* retVal = mObjects.front();
			mObjects.pop_front();
			return retVal;
		}
		else
		{
			for (int i = 0; i < SIZE; ++i) {
				T* newObject = new T();
				mObjects.push_back(newObject);
			}
			T* retVal = mObjects.front();
			mObjects.pop_front();
			return retVal;
		}
			return nullptr;
	}
	void ReturnObject(T* object)
	{
		mObjects.push_back(object);
	}
private:
	std::list<T*> mObjects;
};
