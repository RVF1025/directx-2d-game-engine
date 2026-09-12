#pragma once
#include <list>
#include <vector>
#include <memory>

template<class T, int SIZE = 100>
class ObjectPool // 메모리 누수 방지를 위해 unique_ptr 기반 오브젝트풀로 수정함
{
public:
	ObjectPool()
	{
		Refill();
	}
	T* PopObject()
	{
		if (mObjects.empty())
		{
			Refill();
		}

		T* retVal = mObjects.front();
		mObjects.pop_front();
		return retVal;
	}
	void ReturnObject(T* object)
	{
		mObjects.push_back(object);
	}
private:
	void Refill()
	{
		for (int i = 0; i < SIZE; ++i) {
			std::unique_ptr<T> newObject = std::make_unique<T>();
			mObjects.push_back(newObject.get());
			mUniqueObjects.push_back(std::move(newObject));
		}
	}

	std::vector<std::unique_ptr<T>> mUniqueObjects;
	std::list<T*> mObjects;
};
