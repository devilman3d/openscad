#pragma once

#include <stdint.h>
#include <map>
#include <list>
#include <stack>
#include "NodeVisitor.h"
#include "Tree.h"
#include "progress.h"

// MinGW defines sprintf to libintl_sprintf which breaks usage of the
// Qt sprintf in QString. This is skipped if sprintf and _GL_STDIO_H
// is already defined, so the workaround defines sprintf as itself.
#ifdef __MINGW32__
#define _GL_STDIO_H
#define sprintf sprintf
#endif
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/smart_ptr/detail/spinlock_pool.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>

#include <memory>
#include <atomic>

// forward declaration: node specific data for threaded traversal
class TraverseData;
// forward declaration: custom cache to ensure geometries aren't deleted prematurely
class TraverseCache;

class ThreadedNodeVisitor : public NodeVisitor
{
	bool threaded;												// indicates this visitor should actually use threads
	typedef boost::detail::spinlock_pool<8> runner_lock;		// locks access to the runners
	boost::interprocess::interprocess_semaphore ready_event;	// set when the first runner has finished
	std::list<TraverseData*> finished;							// a list of finished runners
	TraverseCache *cache;										// custom cache to ensure geometries aren't deleted prematurely

	const Tree &tree;
	Progress &progress;

	// waits for any runners to finish and fills finished with 'em
	// called on the main thread
	void waitForAny(std::list<TraverseData*> &finished);

	// start runners using the available cores and wait for them to finish
	Response waitForIt(TraverseData *nodeData);

protected:
	bool smartCacheInsert(const AbstractNode &node, shared_ptr<const Geometry> geom);
	bool checkSmartCache(const AbstractNode &node, shared_ptr<const Geometry> &geom);

	Response runPrefix(const AbstractNode &node, const class State &state, TraverseData *parentData, size_t currentDepth);

	static boost::detail::spinlock cacheLock;

public:
  ThreadedNodeVisitor(const Tree &tree, Progress &progress, bool threaded = false)
	  : threaded(threaded), ready_event(0), cache(NULL), tree(tree), progress(progress) {
  }
  virtual ~ThreadedNodeVisitor() { }

  Response traverseThreaded(const AbstractNode &node);

  // posts ready_event if this is the first and moves it to finished
  // called on the runner thread
  void finishRunner(TraverseData *runner);

  const Tree &getTree() const { return tree; }
  Progress &getProgress() const { return progress; }
};
