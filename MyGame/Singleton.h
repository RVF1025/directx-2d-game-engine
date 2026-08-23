#pragma once

template <class T>

class Singleton
{
private:
	static T* m_Single;

public:
	Singleton()
	{

	};

	virtual ~Singleton()
	{
		delete m_Single;
	};

	static T* GetInstance()
	{
		if (m_Single == nullptr)
		{
			m_Single = new T();
		}

		return m_Single;
	};
};

template<class T>T* Singleton<T>::m_Single = nullptr;