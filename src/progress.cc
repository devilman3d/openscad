#include "progress.h"

boost::thread_specific_ptr<CpuProgress> CpuProgress::progressForThread;

void Progress::tick()
{
	this->value = this->value + 1;
	if (impl)
		impl->progress_changed();
}

void Progress::setCount(int count)
{
	this->value = 0;
	this->count = count;
	if (impl)
		impl->progress_changed();
}

void Progress::throwIfCancelled()
{
	if (impl && impl->progress_is_canceled())
		throw ProgressCancelException();
}

void Progress::setCpuProgress(int cpuId, const shared_ptr<CpuProgressData> &cp)
{
	this->cpuData[cpuId] = cp;
	if (impl)
		impl->progress_changed();
}