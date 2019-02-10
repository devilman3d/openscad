#pragma once

#include <atomic>
#include "memory.h"
#include "Geometry.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/tss.hpp>

class ProgressCancelException { };

class IProgress
{
public:
	virtual void progress_changed() = 0;
	virtual bool progress_is_canceled() = 0;
};

struct CpuProgressData
{
	int cpuId;
	std::string description;
	int value;
	int maxValue;

	CpuProgressData(int cpuId, const std::string &description, int value, int maxValue)
		: cpuId(cpuId), description(description), value(value), maxValue(maxValue)
	{
	}
};

template <typename T>
struct ChangeValue
{
	bool changed;
	T value;

	ChangeValue() 
		: changed(true)
	{
	}

	ChangeValue(const T &value)
		: changed(true), value(value)
	{
	}

	operator const T&() const
	{
		return value;
	}

	bool isChanged()
	{
		bool result = this->changed;
		this->changed = false;
		return result;
	}

	bool isChanged(T &current)
	{
		current = this->value;
		return isChanged();
	}
};

class Progress
{
public:
	struct PreviewGeometries
	{
		NodeGeometries solid;
		NodeGeometries hilite;
		NodeGeometries background;
	};

	Progress(IProgress *impl = nullptr) : count(0), value(0), impl(impl)
	{
		cpuData.resize(boost::thread::hardware_concurrency());
	}

	ChangeValue<int> count;
	ChangeValue<int> value;
	std::vector<ChangeValue<shared_ptr<CpuProgressData>>> cpuData;

	IProgress *impl;

	shared_ptr<const Geometry> activeGeom;

	bool isCanceled() const { return impl ? impl->progress_is_canceled() : false; }

	void tick();
	void setCount(int count);
	void throwIfCancelled();
	void setCpuProgress(int cpuId, const shared_ptr<CpuProgressData> &cp);
};

// Provides access to CpuProgressData on a thread associated with a "cpu id"
class CpuProgress
{
	static boost::thread_specific_ptr<CpuProgress> progressForThread;

public:
	static CpuProgress *getCurrent() { return progressForThread.get(); }

public:
	CpuProgress(Progress *progress, int cpuId, const std::string &name) 
		: progress(progress), cpuId(cpuId)
	{
		progressForThread.reset(this);
		CpuProgressData state(cpuId, name, 0, 0);
		stateStack.push_back(state);
		update();
	}

	~CpuProgress()
	{
		progress->setCpuProgress(cpuId, nullptr);
		progressForThread.release();
	}

	void update(bool throwIfCanceled = false)
	{
		auto data = std::make_shared<CpuProgressData>(stateStack.back());
		progress->setCpuProgress(cpuId, data);
		if (throwIfCanceled)
			progress->throwIfCancelled();
	}

	void push(const std::string &name, int maxValue)
	{
		CpuProgressData state(cpuId, name, 0, maxValue);
		stateStack.push_back(state);
		update();
	}

	void pop()
	{
		stateStack.pop_back();
		update();
	}

	void setCount(int maxValue)
	{
		stateStack.back().maxValue = maxValue;
		update();
	}

	void tick(bool throwIfCanceled = false)
	{
		stateStack.back().value++;
		update(throwIfCanceled);
	}

	void finish()
	{
		stateStack.back().value = stateStack.back().maxValue ? stateStack.back().maxValue : 1;
		update();
	}

	Progress *progress;
	int cpuId;
	std::list<CpuProgressData> stateStack;
};

// RAII class to wrap access to the CpuProgress for the current thread
class LocalProgress
{
	CpuProgress *progress;
public:
	LocalProgress(const std::string &name, int maxValue = 0)
		: progress(CpuProgress::getCurrent())
	{
		if (progress)
			progress->push(name, maxValue);
	}
	~LocalProgress()
	{
		if (progress) {
			progress->pop();
			progress = nullptr;
		}
	}

	void setCount(int maxValue)
	{
		if (progress)
			progress->setCount(maxValue);
	}

	void tick()
	{
		if (progress)
			progress->tick(true);
	}
};
