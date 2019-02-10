#include "ThreadedNodeVisitor.h"
#include "node.h"
#include "ModuleInstantiation.h"
#include "state.h"
#include "feature.h"
#include "printutils.h"
#include <algorithm>

#include <boost/thread.hpp>
#include <stack>
#include <map>
#include <string>

#include "Tree.h"
#include "progress.h"
#include "cgalutils.h"
#include "CGALCache.h"
#include "GeometryCache.h"

#define QT_STATIC
#include <QTime>
#include <qdatetime.h>

#define ENABLE_TRAVERSE_CACHE

const char *ResponseStr[3] = { "Continue", "Abort", "Prune" };

// a global lock to guard access to the cache
boost::detail::spinlock ThreadedNodeVisitor::cacheLock = BOOST_DETAIL_SPINLOCK_INIT;

class comma_numpunct : public std::numpunct<char>
{
protected:
	//virtual char do_thousands_sep() const { return ','; }
	virtual std::string do_grouping() const { return "\03"; }
};

template <class T>
std::string commas(T i)
{
	std::stringstream ss;
	std::locale loc(ss.getloc(), new comma_numpunct());
	ss.imbue(loc);
	ss << i;
	return ss.str();
}

std::string timeStr(double t)
{
	auto hh = (int)(t / 3600);
	t -= hh * 3600;
	auto mm = (int)(t / 60);
	t -= mm * 60;
	std::stringstream str;
	if (hh > 0)
		str << boost::format("%d:%0.2d:%0.2f") % hh % mm % t;
	else if (mm > 0)
		str << boost::format("%d:%0.2f") % mm % t;
	else
		str << boost::format("%0.2f") % t;
	return str.str();
}

class TraverseData
{
	enum TraverseDataState
	{
		NONE,
		PREFIXED,
		RUNNING,
		FINISHED
	};
	boost::thread *thread;
	TraverseData *parent;
	std::string idString;
	int cpuId;
	const AbstractNode *node;
	State state;
	size_t depth;
	Response response;
	TraverseDataState dataState;
	double elapsed;
	std::list<TraverseData*> children;

public:
	TraverseData(const std::string &idString, const AbstractNode *node, const State &state, size_t depth)
		: thread(nullptr)
		, parent(NULL)
		, idString(idString)
		, cpuId(0)
		, node(node)
		, state(state)
		, depth(depth)
		, response(ContinueTraversal)
		, dataState(NONE)
		, elapsed(0)
	{
	}

	~TraverseData()
	{
		if (thread)
			delete thread;
		for (auto child : children)
			delete child;
	}

	TraverseData *getParent() const { return parent; }
	const std::string &getIdString() const { return idString; }
	int getCpuId() const { return cpuId; }
	const AbstractNode *getNode() const { return node; }
	const State &getState() const { return state; }
	const std::list<TraverseData*> &getChildren() const { return children; }
	int getId() const { return node->index(); }
	size_t getDepth() const { return depth; }
	Response getResponse() const { return response; }
	double getElapsed() const { return elapsed; }

	void addChild(TraverseData *data)
	{
		data->parent = this;
		children.push_back(data);
	}

	TraverseData *getRunner(ThreadedNodeVisitor *traverser,
		const std::map<std::string, TraverseData*> &running, 
		bool &lastLeaf)
	{
		TraverseData *result = NULL;
		lastLeaf = false;
		if (dataState < RUNNING)
		{
			bool hasUnfinishedChild = false;
			for (auto child : children)
			{
				if (child->dataState != FINISHED)
					hasUnfinishedChild = true;
				bool dummy = false;
				result = child->getRunner(traverser, running, dummy);
				if (result != NULL)
					break;
			}
			if (!hasUnfinishedChild && result == NULL)
			{
				//PRINTB(" No unfinished children: %s", toString());
				if (running.find(idString) == running.end())
				{
					//PRINTB("++ Creating runner: %s", toString());
					dataState = RUNNING;
					result = this;
					lastLeaf = true;
				}
			}
		}
		return result;
	}

	void startThread(ThreadedNodeVisitor &visitor, int cpuId)
	{
		auto f = [this, &visitor]
		{
			try {
				// set the current thread's progress object
				CpuProgress progress(&visitor.getProgress(), this->cpuId, this->node->name());
				//PRINTB("  (%d) Running postfix", data->getId());
				this->accept(true, visitor);
			}
			catch (const ProgressCancelException &c) {
				// eat it...
			}
			visitor.finishRunner(this);
			//PRINTB("  (%d) Finished postfix: %s", data->getId() % ResponseStr[data->getResponse()]);
		};
		this->cpuId = cpuId;
		this->thread = new boost::thread(f);
	}

	size_t countUnprunedLeaves() const
	{
		size_t result = 1;
		if (response == ContinueTraversal) {
			for (auto child : children)
				result += child->countUnprunedLeaves();
		}
		return result;
	}

	size_t countGeometries(const AbstractNode *node = NULL) const
	{
		size_t result = 1;
		auto kids = node == NULL ? this->node->getChildren() : node->getChildren();
		for (auto child : kids)
			result += countGeometries(child.get());
		return result;
	}

	Response accept(bool postfix, ThreadedNodeVisitor &visitor)
	{
		QTime qTimer;
		qTimer.start();
		try
		{
			if (response != AbortTraversal)
			{
				state.setPrefix(!postfix);
				state.setPostfix(postfix);
				response = node->accept(state, visitor);
				if (postfix)
					if (auto progress = CpuProgress::getCurrent())
						progress->finish();
			}
		}
		catch (const ProgressCancelException &c)
		{
			PRINT("!!! Cancelling node traversal !!!");
			response = AbortTraversal;
		}
		catch (const std::exception &ex)
		{
			PRINTB("!!! Exception traversing node: %s", ex.what());
			response = AbortTraversal;
		}
		catch (...)
		{
			PRINT("!!! Unhandled exception traversing node");
			response = AbortTraversal;
		}
		if (postfix) {
			dataState = FINISHED;
			elapsed = qTimer.elapsed();
		}
		else
			dataState = PREFIXED;
		return response;
	}

	std::string toNodeIdString() const
	{
		std::stringstream str;
		if (parent != NULL)
			str << parent->toNodeIdString() << ":";
		str << node->index();
		return str.str();
	}

	std::string toString() const
	{
		std::stringstream str;
		str << node->name() << " #" << toNodeIdString() << " at depth " << depth;
		return str.str();
	}
};

class TraverseCache
{
	struct CacheItem
	{
		std::string idString;
		size_t totalRefs;
		size_t deadRefs;
		size_t insertedRefs;
		size_t prunedRefs;
		size_t memorySize;
		std::shared_ptr<const Geometry> geom;

		// add a reference count
		void addRef()
		{
			totalRefs++; 
		}

		// sets the geometry; return the memory size delta
		int setGeom(shared_ptr<const Geometry> geom)
		{
			if (this->geom == geom)
				return 0;
			size_t geomSize = geom->isEmpty() ? 0 : geom->memsize();
			int result = (int)geomSize;
			if (insertedRefs > 0)
			{
				PRINTB("Replacing cached geometry with something else: new size=%1%, size=%2%, refs=%3%, total=%4%, dead=%5%, pruned=%6%",
					commas(geomSize) % commas(memorySize) % insertedRefs % totalRefs % deadRefs % prunedRefs);
				result -= (int)memorySize;
			}
			this->geom = geom;
			this->memorySize = geomSize;
			this->insertedRefs++;
			return result;
		}

		size_t pruneRef()
		{
			// release the geometry if it's the last one
			if (++prunedRefs + deadRefs == totalRefs)
				return memorySize;
			return 0;
		}

		size_t releaseRef()
		{
			// release the geometry if it's the last one
			if (++deadRefs + prunedRefs == totalRefs)
				return memorySize;
			return 0;
		}

		bool isAlive() const
		{
			return insertedRefs > 0 && (deadRefs + prunedRefs) != totalRefs;
		}

		CacheItem(std::string _idString)
			: idString(_idString)
			, totalRefs(0)
			, deadRefs(0)
			, insertedRefs(0)
			, prunedRefs(0)
			, memorySize(0)
		{
		}

		~CacheItem()
		{
			if (geom)
			{
				shared_ptr<const CGAL_Nef_polyhedron> cgalgeom = 
					dynamic_pointer_cast<const CGAL_Nef_polyhedron>(geom);
				if (cgalgeom)
					CGALCache::instance()->insert(idString, cgalgeom);
				else
					GeometryCache::instance()->insert(idString, geom);
				geom.reset();
			}
		}
	};

	size_t prune(const std::string &idString)
	{
		auto iter = cache.find(idString);
		assert(iter != cache.end());
		CacheItem *item = iter->second;
		size_t delta = item->pruneRef();
		if (delta != 0)
		{
			pruneMemory += delta;
			pruneCount++;
			delete item;
			cache.erase(idString);
		}
		pruneTotal++;
		return delta;
	}

	void add(const std::string &idString)
	{
		auto iter = cache.find(idString);
		if (iter == cache.end())
		{
			cache.insert(std::make_pair(idString, new CacheItem(idString)));
			iter = cache.find(idString);
		}
		iter->second->addRef();
		totalRefs++;
	}

	void addNode(const Tree &tree, const AbstractNode &node)
	{
		const std::string &idString = tree.getIdString(node);
		add(idString);
		if (CGALCache::instance()->contains(idString))
		{
			shared_ptr<const Geometry> cached = CGALCache::instance()->get(idString);
			int size = insert(idString, cached);
			if (size != 0)
			{
				precacheCount++;
				precacheMemory += size;
			}
			precacheTotal++;
		}
		//else if (GeometryCache::instance()->contains(idString))
		//{
		//	shared_ptr<const Geometry> cached = GeometryCache::instance()->get(idString);
		//	int size = insert(idString, cached);
		//	if (size != 0)
		//	{
		//		precacheCount++;
		//		precacheMemory += size;
		//	}
		//	precacheTotal++;
		//}
		if (!node.getChildren().empty())
		{
			for (auto child : node.getChildren())
				addNode(tree, *child);
		}
		else
			totalLeafs++;
	}

	std::map<std::string, CacheItem*> cache;
	size_t memorySize;
	size_t pruneMemorySize;
	size_t peakMemorySize;
	size_t totalRefs;
	size_t totalLeafs;
	size_t precacheCount;
	size_t precacheTotal;
	size_t precacheMemory;
	size_t pruneCount;
	size_t pruneTotal;
	size_t pruneMemory;

public:
	TraverseCache(const Tree &tree, const AbstractNode &node)
		: memorySize(0)
		, pruneMemorySize(0)
		, peakMemorySize(0)
		, totalRefs(0)
		, totalLeafs(0)
		, precacheCount(0)
		, precacheTotal(0)
		, precacheMemory(0)
		, pruneCount(0)
		, pruneTotal(0)
		, pruneMemory(0)
	{
		addNode(tree, node);
	}

	~TraverseCache()
	{
		for (auto item : cache)
		{
			if (item.second->isAlive())
			{
				auto cc = item.second;
				PRINTDB("Traverse cache living object: size=%1%, refs=%2%, total=%3%, dead=%4%: %5%",
					commas(cc->memorySize) % cc->insertedRefs % cc->totalRefs % cc->deadRefs % cc->idString.substr(0, 80));
			}
			delete item.second;
		}
		cache.clear();
	}

	void printPrecache() const
	{
		PRINT("Precache:");
		PRINTB("    Precache items: shared=%1%, total=%2%", commas(precacheCount) % commas(precacheTotal));
		PRINTB("    Precache size: %1%", commas(precacheMemory));
		PRINTB("    Reserved items: shared=%1%, total=%2%", commas(cache.size()) % commas(totalRefs));
	}

	void printPrunecache() const
	{
		PRINT("Prune cache:");
		PRINTB("    Pruned items: shared=%1%, total=%2%", commas(pruneCount) % commas(pruneTotal));
		PRINTB("    Pruned size: %1%", commas(pruneMemory));
		PRINTB("    Unpruned items: shared=%1%, total=%2%", commas(precacheCount - pruneCount) % commas(totalRefs - pruneTotal));
		PRINTB("    Unpruned size: %1%", commas(precacheMemory));
		PRINTB("    Final items: shared=%1%, total=%2%", commas(cache.size()) % commas(totalRefs - pruneTotal));
	}

	void print() const
	{
		PRINT("Final cache:");
		PRINTB("    Peak size: %1%", commas(peakMemorySize));
		PRINTB("    Unpruned size: %1%", commas(memorySize));
		PRINTB("    Pruned size: %1%", commas(memorySize - pruneMemorySize));
	}

	// prune the children of the given node
	void pruneChildren(const Tree &tree, const AbstractNode *node)
	{
		for (auto child : node->getChildren())
		{
			pruneChildren(tree, child.get());
			std::string idString = tree.getIdString(*child);
			size_t delta = prune(idString);
			if (delta != 0)
			{
				pruneMemorySize += delta;
				PRINTDB("Traverse cache prune: %1%, total=%2%: %3%", commas(delta) % commas(pruneMemorySize) % idString.substr(0, 80));
			}
		}
	}

	int insert(const std::string &idString, shared_ptr<const Geometry> geom)
	{
		auto iter = cache.find(idString);
		assert(iter != cache.end());
		int delta = iter->second->setGeom(geom);
		if (delta != 0)
		{
			memorySize += delta;
			PRINTDB("Traverse cache insert: %1%, total=%2%: %3%", commas(delta) % commas(memorySize) % idString.substr(0, 80));
		}
		if (memorySize > peakMemorySize)
			peakMemorySize = memorySize;
		return delta;
	}

	void release(const std::string &idString)
	{
		auto iter = cache.find(idString);
		assert(iter != cache.end());
		size_t delta = iter->second->releaseRef();
		if (delta != 0)
		{
			delete iter->second;
			cache.erase(idString);
			memorySize -= delta;
			PRINTDB("Traverse cache release: %1%, total=%2%: %3%", commas(delta) % commas(memorySize) % idString.substr(0, 80));
		}
	}

	bool get(const std::string &idString, shared_ptr<const Geometry> &geom) const
	{
		auto iter = cache.find(idString);
		assert(iter != cache.end());
		if (iter->second->isAlive())
		{
			geom = iter->second->geom;
			PRINTDB("Traverse cache hit: %s", idString.substr(0, 80));
			return true;
		}
		return false;
	}
};

bool ThreadedNodeVisitor::checkSmartCache(const AbstractNode &node, shared_ptr<const Geometry> &geom)
{
	bool result = false;
#ifdef ENABLE_TRAVERSE_CACHE
	if (cache != NULL)
	{
		boost::detail::spinlock::scoped_lock lock(cacheLock);
		result = cache->get(tree.getIdString(node), geom);
	}
#endif
	return result;
}

bool ThreadedNodeVisitor::smartCacheInsert(const AbstractNode &node, shared_ptr<const Geometry> geom)
{
#ifdef ENABLE_TRAVERSE_CACHE
	if (cache != NULL)
	{
		boost::detail::spinlock::scoped_lock lock(cacheLock);
		cache->insert(tree.getIdString(node), geom);
		return true;
	}
#endif
	return false;
}

Response ThreadedNodeVisitor::traverseThreaded(const AbstractNode &node)
{
	if (!threaded)
		return traverse(node);

	TraverseCache localCache(tree, node);
	cache = &localCache;

	State state(nullptr);
	state.setNumChildren(node.getChildren().size());

	std::string idString = tree.getIdString(node);
	TraverseData nodeData(idString, &node, state, 0);

	// get the updated node state;
	State nodeState = nodeData.getState();

#ifdef ENABLE_TRAVERSE_CACHE
	cache->printPrecache();
#endif
	size_t geomCount = nodeData.countGeometries();
	PRINTB("Threaded traversal phase 1: Prefix %d nodes", geomCount);

	//PRINTB("  (%d) Running prefix", nodeData->getId());
	Response response = nodeData.accept(false, *this);
	//PRINTB("  (%d) Finished prefix: %s", nodeData->getId() % ResponseStr[nodeData->getResponse()]);

	// abort if the node aborted
	if (response == AbortTraversal)
		return response;

	// Pruned traversals mean don't traverse children
	if (response == PruneTraversal && cache != NULL)
	{
#ifdef ENABLE_TRAVERSE_CACHE
		cache->pruneChildren(tree, &node);
#endif
	}
	else if (response == ContinueTraversal)
	{
		// recursively traverse the node's children
		for (auto chnode : node.getChildren()) {
			State chstate = nodeState;
			chstate.setParent(&node, nodeState);
			response = runPrefix(*chnode, chstate, &nodeData, 1);
			if (response == AbortTraversal)
				break;
		}
	}

	// abort if any child aborted
	if (response == AbortTraversal)
		return response;

#ifdef ENABLE_TRAVERSE_CACHE
	cache->printPrunecache();
#endif

	// multithreaded postfixing
	response = waitForIt(&nodeData);

#ifdef ENABLE_TRAVERSE_CACHE
	cache->release(nodeData.getIdString());
	cache->print();
#endif

	if (response == AbortTraversal)
		return response;

	return ContinueTraversal;
}

Response ThreadedNodeVisitor::runPrefix(const AbstractNode &node, const State &parentState, TraverseData *parentData, size_t currentDepth)
{
	State state = parentState;
	state.setNumChildren(node.getChildren().size());

	std::string idString = tree.getIdString(node);
	TraverseData *nodeData = new TraverseData(idString, &node, state, currentDepth);
	parentData->addChild(nodeData);

	Response response = nodeData->accept(false, *this);

	State nodeState = nodeData->getState();
		
	// abort if the node aborted
	if (response == AbortTraversal)
		return response;

	// Pruned traversals mean don't traverse children
	if (response == PruneTraversal)
	{
		if (cache != NULL)
		{
#ifdef ENABLE_TRAVERSE_CACHE
			cache->pruneChildren(tree, &node);
#endif
		}
	}
	else if (response == ContinueTraversal)
	{
		// recursively prefix the node's children
		for (auto chnode : node.getChildren()) {
			// state for child nodes
			State state = nodeState;
			state.setParent(&node, nodeState);
			// recurse the child
			response = runPrefix(*chnode, state, nodeData, currentDepth + 1);
			if (response == AbortTraversal)
				break;
		}
	}

	// abort if any child aborted
	if (response == AbortTraversal)
		return response;

	return ContinueTraversal;
}

// start runners using the available cores and wait for them to finish
Response ThreadedNodeVisitor::waitForIt(TraverseData *nodeData)
{
	Response response = ContinueTraversal;
	// lock CGAL errors on the main thread
	// this allows the spawned threads to catch their CGAL exceptions and not crash the whole app
	// Response::AbortTraversal is "bubbled-up" when it occurs
	CGALUtils::ErrorLocker locker;
	size_t maxThreads = boost::thread::hardware_concurrency();
	size_t leafCount = nodeData->countUnprunedLeaves();
	progress.setCount((int)leafCount);
	PRINTB("Threaded traversal phase 2: Spawning %d threads on %d logical CPUs", leafCount % maxThreads);
	size_t leafCounter = 0;
	size_t totalJoinCount = 0;
	std::map<std::string, TraverseData*> running;
	std::vector<TraverseData*> usedCpus;
	usedCpus.resize(maxThreads);
	bool lastLeaf = false;
	double threadTime = 0;
	QTime qTimer;
	qTimer.start();
	// wait loop
	do
	{
		// step 1: start runners unless aborted or found the last leaf or enough are running
		if (response != AbortTraversal && !lastLeaf && running.size() < maxThreads)
		{
			while (!lastLeaf && running.size() < maxThreads)
			{
				TraverseData *runner = nodeData->getRunner(this, running, lastLeaf);
				if (runner == NULL)
				{
					assert(!lastLeaf && "Why doesn't the last leaf have data???");
					// we get here when a single parent is waiting on one or more children to complete
					break;
				}
				leafCounter++;
				if (lastLeaf)
				{
					//PRINTB("That's the last leaf (erm, the root)!!! counted=%d, count=%d", leafCounter % leafCount);
					assert(leafCount == leafCounter && "Why don't the leaf counts match???");
					assert(nodeData == runner && "Why isn't the last leaf its own data???");
				}
				// find the first available "cpu" [for UI]
				int cpuId = 0;
				for (int i = 0; i < usedCpus.size(); ++i) {
					if (!usedCpus[i]) {
						cpuId = i;
						usedCpus[i] = runner;
						break;
					}
				}
				// put the runner into the running map
				running[runner->getIdString()] = runner;
				// start the thread
				runner->startThread(*this, cpuId);
			}
		}
		// if running is empty, no threads are running and no new threads were added = done!
		if (running.empty())
		{
			//PRINT("No more pending children");
			assert(totalJoinCount == leafCounter && "Why weren't as many threads joined as started???");
			break;
		}
		// step 2: wait for any children to finish and do housekeeping
		//PRINT("Waiting for finished children");
		std::list<TraverseData*> finished;
		waitForAny(finished);
		size_t joinCount = 0;
		for (auto runner : finished)
		{
			// reset the cpu flag
			usedCpus[runner->getCpuId()] = nullptr;
			// check for abort
			if (runner->getResponse() == AbortTraversal)
				response = AbortTraversal;
#ifdef ENABLE_TRAVERSE_CACHE
			// release cached references for the node's children
			{
				boost::detail::spinlock::scoped_lock lock(cacheLock);
				for (auto child : runner->getChildren()) {
					cache->release(child->getIdString());
				}
			}
#endif
			// update the thread time accumulator
			threadTime += runner->getElapsed() / 1000.0;
			// pull it from running
			running.erase(runner->getIdString());
			// increment the join counts
			joinCount++;
			totalJoinCount++;
			// tick the main progress
			progress.tick();
		}
		/*
		if (joinCount > 0 && runningCount != 0)
		{
			std::stringstream ss;
			bool first = true;
			for (auto runner : running)
			{
				if (!first)
					ss << ", ";
				ss << runner->second.data->getId();
				first = false;
			}
			PRINTB("Joined %d threads (%d/%d), still running: (%d) %s", joinCount % totalJoinCount % leafCount % runningCount % ss.str());
		}
		*/
	} while (true); // wait loop

	double totalTime = qTimer.elapsed() / 1000.0;
	double mult = totalTime == 0 ? 1.0 : threadTime / totalTime;
	PRINTB("Threaded traversal finished: time in threads=%s / wall time=%s = %1.2fx",
		timeStr(threadTime) % timeStr(totalTime) % mult);

	return response;
}

// posts ready_event if this is the first and moves it to finished
// called on the runner thread
void ThreadedNodeVisitor::finishRunner(TraverseData *runner)
{
	runner_lock::scoped_lock lock(this);
	// post ready_event if this is the first runner to finish
	if (finished.empty())
		ready_event.post();
	// move it to finished
	finished.push_back(runner);
}

// waits for any runners to finish and fills finished with 'em
// called on the main thread
void ThreadedNodeVisitor::waitForAny(std::list<TraverseData*> &finished)
{
	ready_event.wait();
	// ready_event was posted; fill the result lists
	runner_lock::scoped_lock lock(this);
	std::swap(finished, this->finished);
}
