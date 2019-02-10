#include "cgalworker.h"
#include <QThread>

#include "Tree.h"
#include "GeometryEvaluator.h"
#include "progress.h"
#include "printutils.h"

CGALWorker::CGALWorker()
{
	connect(&thread, SIGNAL(started()), this, SLOT(work()));
	if (thread.stackSize() < 1024*1024) 
		thread.setStackSize(1024*1024);
	moveToThread(&thread);
}

CGALWorker::~CGALWorker()
{
}

void CGALWorker::start(const Tree &tree, Progress &progress)
{
	this->tree = &tree;
	this->progress = &progress;
	this->thread.start();
}

void CGALWorker::work()
{
	shared_ptr<const Geometry> root_geom;
	try {
		GeometryEvaluator geomevaluator(*this->tree, *this->progress, true, true);
		root_geom = geomevaluator.evaluateGeometry(*this->tree->root());
	}
	catch (const ProgressCancelException &e) {
		PRINT("Rendering cancelled.");
	}

	emit done(root_geom);
	thread.quit();
}
