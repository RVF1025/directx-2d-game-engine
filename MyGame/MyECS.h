#pragma once

#include "Singleton.h"
#include "ObjectPool.h"
#include "Definitions.h"

#include <unordered_map>
#include <typeindex>
#include <functional>
#include <algorithm>
#include <memory>
#include <set>

class Manager;
class System;

template <class T>
class EventSubscriber;

// Component / Event 인터페이스
struct IComponent
{
	virtual ~IComponent() = default;
};

struct IEvent
{
	virtual ~IEvent() = default;
};

// 템플릿 인스턴스마다 static 변수가 한번만 생성되는 것을 이용한 ID 생성기
using ComponentID = uint32_t;
using SystemID = uint32_t;
using EventID = uint32_t;

inline ComponentID GetNextComponentID()
{
	static ComponentID id = 0;
	return id++;
}

template <class T>
ComponentID GetComponentID()
{
	static ComponentID id = GetNextComponentID();
	return id;
}

inline SystemID GetNextSystemID()
{
	static SystemID id = 0;
	return id++;
}

template <class T>
SystemID GetSystemID()
{
	static SystemID id = GetNextSystemID();
	return id;
}

inline EventID GetNextEventID()
{
	static EventID id = 0;
	return id++;
}

template <class T>
EventID GetEventID()
{
	static EventID id = GetNextEventID();
	return id;
}

// 기존의 IsHavingComponent 연산을 효율적으로 하기 위한 커스텀 Bitmask 구조체
// 캐시미스를 줄이기 위해 64비트 동적 Bitset을 사용해보는걸로
using BitBlock = uint64_t;

class ComponentBitmask
{
public:

	void Add(ComponentID id)
	{
		size_t requiredBlock = id / 64;

		if (requiredBlock >= blocks.size())
			blocks.resize(requiredBlock + 1);

		blocks[id / 64] |= (1ull << (id % 64));
	}

	void Remove(ComponentID id)
	{
		size_t requiredBlock = id / 64;

		if (requiredBlock >= blocks.size())
			return;

		blocks[requiredBlock] &= ~(1ull << (id % 64));

		while (!blocks.empty() && blocks.back() == 0)
		{
			blocks.pop_back();
		}
	}

	void Clear()
	{
		blocks.clear();
	}

	bool HasComponent(const ComponentBitmask& required) const
	{
		if (blocks.size() < required.blocks.size())
			return false;

		for (size_t i = 0; i < required.blocks.size(); ++i)
		{
			if ((blocks[i] & required.blocks[i]) != required.blocks[i])
				return false;
		}

		return true;
	}

private:
	std::vector<BitBlock> blocks;
};

template <class... Types>
static const ComponentBitmask& GetComponentBitmask()
{
	// ComponentID와 비슷하게 타입 조합별로 Bitmask 한번씩만 생성
	static ComponentBitmask mask = [] {
		ComponentBitmask m;
		(m.Add(GetComponentID<Types>()), ...);
		return m;
	}();
	return mask;
}

struct DeferredEventData
{
	EventID id; 
	std::unique_ptr<IEvent> data;
};

struct SubscriberData
{
	SystemID id;
	void (*invoke)(Manager&, System*, void*);
};

template <class TypeSystem, class TypeEvent>
static void InvokeEvent(Manager& manager, System* system, void* event)
{
	static_assert(std::is_base_of_v<System, TypeSystem>, "Invalid System");
	static_assert(std::is_base_of_v<EventSubscriber<TypeEvent>, TypeSystem>, "Invalid Subscriber");

	auto* sys = static_cast<TypeSystem*>(system);
	auto* sub = static_cast<EventSubscriber<TypeEvent>*>(sys);
	auto* ev = static_cast<TypeEvent*>(event);
	sub->Received(manager, ev);
}

class Entity
{
public:
	friend class Manager;

	Entity() { };
	~Entity() { };

	template<class T, class... Args>
	void AddComponent(Args&&... args)
	{
		ComponentID id = GetComponentID<T>();

		if (id >= mComponents.size())
			mComponents.resize(id + 1);

		mComponents[id] = std::make_unique<T>(std::forward<Args>(args)...);
		mComponentBitmask.Add(id);
	}

	template <class T>
	T* GetComponent()
	{
		ComponentID id = GetComponentID<T>();

		if (id >= mComponents.size())
			return nullptr;

		return static_cast<T*>(mComponents[id].get());
	}

	template <class T>
	bool RemoveComponent()
	{
		ComponentID id = GetComponentID<T>();

		if (id < mComponents.size())
		{
			mComponents[id].reset();
			mComponentBitmask.Remove(id);
			return true;
		}

		return false;
	}

	void RemoveAllComponents()
	{
		mComponents.clear();
		mComponentBitmask.Clear();
	}

	template <class... Types>
	bool IsHavingComponent() const
	{
		const ComponentBitmask& requiredComponents = GetComponentBitmask<Types...>();

		return mComponentBitmask.HasComponent(requiredComponents);
	}

	bool IsPendingDestroy()
	{
		return bPendingDestroy;
	}

	void SetPendingDestroy(bool b)
	{
		bPendingDestroy = b;
	}

private:
	std::vector<std::unique_ptr<IComponent>> mComponents;
	ComponentBitmask mComponentBitmask;
	bool bPendingDestroy = false;
};

struct OnEntityCreated : IEvent
{
	OnEntityCreated(Entity* ent) : m_Entity(ent) {};

	Entity* m_Entity;
};

struct OnEntityDestroyed : IEvent
{
	OnEntityDestroyed(Entity* ent) : m_Entity(ent) {};

	Entity* m_Entity;
};

template<class T>
class EventSubscriber
{
public:
	virtual void Received(Manager& manager, T* event) = 0;
};

class System : public EventSubscriber<OnEntityCreated>, public EventSubscriber<OnEntityDestroyed>
{
public:
	System() { }
	~System() { }

	enum SystemType : int
	{
		Logic,
		Render,
	};

	SystemType systemType = Logic;

	virtual void Init(Manager& manager) {}
	virtual void OnActive(Manager& manager) {};
	virtual void Tick(Manager& manager, float deltatime) {}
	virtual void Received(Manager& manager, OnEntityCreated* event) {};
	virtual void Received(Manager& manager, OnEntityDestroyed* event) {};
};

class Manager : public Singleton<Manager>
{
public:
	Manager() { }
	~Manager() { }

	void Tick(float deltatime = 0.0f)
	{
		for (auto& sys : mSystems)
		{
			if (!sys)
				continue;

			if (sys->systemType == System::Render)
				continue;

			sys->Tick(*this, deltatime);
		}
		
		EmitDeferredEvents();

		CleanEntity();
		ClearDeferredEvents();
	}

	void Render(float deltatime = 0.0f)
	{
		for (auto& sys : mSystems)
		{
			if (!sys)
				continue;

			if (sys->systemType == System::Logic)
				continue;

			sys->Tick(*this, deltatime);
		}
	}

	template <class Functor>
	Entity* CreateEntity(Functor f)
	{
		Entity* ent = mEntityPool.PopObject();
		if (ent == nullptr)
			return nullptr;
		f(ent);
		ExecuteEvent<OnEntityCreated>(ent);
		mEntities.emplace_back(ent);
		return ent;
	}
	
	template<class T>
	void RegisterSystemImpl()
	{
		SystemID id = GetSystemID<T>();

		if (id >= mSystems.size())
		{
			mSystems.resize(id + 1);
			mInactivatedSystems.resize(id + 1);
		}

		std::unique_ptr<System> sys = std::make_unique<T>();
		sys->Init(*this);
		sys->systemType = System::Logic;
		SubscribeEvent<T, OnEntityCreated>();
		SubscribeEvent<T, OnEntityDestroyed>();
		mSystems[id] = std::move(sys);
	}

	template<class... Types>
	void RegisterSystem()
	{
		(RegisterSystemImpl<Types>(), ...);
	}

	template<class T>
	void RegisterRenderSystemImpl()
	{
		SystemID id = GetSystemID<T>();

		if (id >= mSystems.size())
		{
			mSystems.resize(id + 1);
		}

		std::unique_ptr<System> sys = std::make_unique<T>();
		sys->Init(*this);
		sys->systemType = System::Render;
		SubscribeEvent<T, OnEntityCreated>();
		SubscribeEvent<T, OnEntityDestroyed>();
		mSystems[id] = std::move(sys);
	}

	template<class... Types>
	void RegisterRenderSystem()
	{
		(RegisterRenderSystemImpl<Types>(), ...);
	}

	template <class T>
	bool ReleaseSystem()
	{
		SystemID id = GetSystemID<T>();

		UnsubscribeEvent<T, OnEntityCreated>();
		UnsubscribeEvent<T, OnEntityDestroyed>();

		if (id < mSystems.size())
		{
			mSystems[id].reset();
			return true;
		}

		return false;
	}

	template <class T>
	bool ReleaseRenderSystem()
	{
		return ReleaseSystem();
	}

	template <class T>
	void ActivateSystem()
	{
		SystemID id = GetSystemID<T>();

		if (id >= mSystems.size())
		{
			mSystems.resize(id + 1);
			mInactivatedSystems.resize(id + 1);
		}

		if (mInactivatedSystems[id])
		{
			mInactivatedSystems[id]->OnActive(*this);
			mSystems[id] = std::move(mInactivatedSystems[id]);
		}
	}

	template <class T>
	void InactivateSystem()
	{
		SystemID id = GetSystemID<T>();

		if (id >= mSystems.size())
		{
			mSystems.resize(id + 1);
			mInactivatedSystems.resize(id + 1);
		}

		if (mSystems[id])
		{
			mInactivatedSystems[id] = std::move(mSystems[id]);
		}
	}

	Entity* GetEntity(size_t index)
	{
		return mEntities[index].get();
	}

	size_t GetEntityNum()
	{
		return mEntities.size();
	}

	void DestroyEntity(Entity* ent, bool isnow = false)
	{
		if (ent == nullptr)
			return;

		if (isnow)
		{
			mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [&](std::unique_ptr<Entity>& e) 
			{
				if (e.get() == ent) 
				{ 
					ExecuteEvent<OnEntityDestroyed>(ent);
					e->RemoveAllComponents();
					mEntityPool.ReturnObject(e.release()); 
					return true; 
				} 
				return false; 
			}), mEntities.end());
		}
		else
		{
			ent->bPendingDestroy = true;
		}
	}

	void CleanEntity()
	{
		mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [&](std::unique_ptr<Entity>& ent) 
		{
			if (ent->IsPendingDestroy()) 
			{
				ExecuteEvent<OnEntityDestroyed>(ent.get());
				ent->RemoveAllComponents();
				ent->SetPendingDestroy(false);
				mEntityPool.ReturnObject(ent.release());
				return true; 
			}  
			return false;
		}), mEntities.end());
	}

	template <class... Types, class Func>
	void Each(Func&& Functor)
	{
		const ComponentBitmask& requiredComponents = GetComponentBitmask<Types...>();

		for (auto& ent : mEntities)
		{
			if (ent->mComponentBitmask.HasComponent(requiredComponents))
			{
				Functor(ent.get(), ent->GetComponent<Types>()...);
			}
		}
	}

	template <class TypeSystem, class TypeEvent>
	void SubscribeEvent()
	{
		EventID eid = GetEventID<TypeEvent>();
		SystemID sid = GetSystemID<TypeSystem>();

		if (eid >= mSubscribers.size())
			mSubscribers.resize(eid + 1);

		if (sid >= mSubscribers[eid].size())
			mSubscribers[eid].resize(sid + 1);

		mSubscribers[eid][sid] = { sid, &InvokeEvent<TypeSystem, TypeEvent> };
	}

	template <class TypeSystem, class TypeEvent>
	void UnsubscribeEvent()
	{
		EventID eid = GetEventID<TypeEvent>();
		SystemID sid = GetSystemID<TypeSystem>();

		if (eid >= mSubscribers.size())
			mSubscribers.resize(eid + 1);

		if (sid >= mSubscribers[eid].size())
			mSubscribers[eid].resize(sid + 1);

		mSubscribers[eid][sid].invoke = nullptr;
	}

	template <class T, class... Args>
	void AddEvent(Args&&... args)
	{
		EventID id = GetEventID<T>();

		mDeferredEvents.push_back({ id, std::make_unique<T>(std::forward<Args>(args)...) });
	}

	template <class T, class... Args>
	void ExecuteEvent(Args&&... args)
	{
		EventID id = GetEventID<T>();

		if (id >= mSubscribers.size())
			mSubscribers.resize(id + 1);
		
		T event(std::forward<Args>(args)...);

		for (auto sub : mSubscribers[id])
		{
			if (!sub.invoke)
				continue;

			System* sys = mSystems[sub.id].get();
			if (!sys)
				continue;

			sub.invoke(*this, sys, &event);
		}
	}

	void EmitDeferredEvents()
	{
		while (!mDeferredEvents.empty())
		{
			std::vector<DeferredEventData> events;
			events.swap(mDeferredEvents);

			for (DeferredEventData& data : events)
			{
				EventID id = data.id;

				if (id >= mSubscribers.size())
					continue;

				for (auto sub : mSubscribers[id])
				{
					if (!sub.invoke)
						continue;

					System* sys = mSystems[sub.id].get();
					if (!sys)
						continue;

					sub.invoke(*this, sys, data.data.get());
				}
			}
		}
	}

	void ClearEntity()
	{
		mEntities.erase(std::remove_if(mEntities.begin(), mEntities.end(), [&](std::unique_ptr<Entity>& ent)
		{
			ExecuteEvent<OnEntityDestroyed>(ent.get());
			ent->RemoveAllComponents();
			mEntityPool.ReturnObject(ent.release());
			return true;
		}), mEntities.end());
	}

	void ClearDeferredEvents()
	{
		mDeferredEvents.clear();
	}

	void ReleaseAll()
	{
		mEntities.clear();
		mSystems.clear();
		mInactivatedSystems.clear();
		mDeferredEvents.clear();
		mSubscribers.clear();
	}

private:
	ObjectPool<Entity> mEntityPool;
	std::vector<std::unique_ptr<Entity>> mEntities;
	std::vector<std::unique_ptr<System>> mSystems;
	std::vector<std::unique_ptr<System>> mInactivatedSystems;
	std::vector<DeferredEventData> mDeferredEvents;
	std::vector<std::vector<SubscriberData>> mSubscribers;
};