#pragma once

#include <QObject>
#include <QThread>
#include "memory.h"

class CGALWorker : public QObject
{
	Q_OBJECT;
public:
	CGALWorker();
	virtual ~CGALWorker();

public slots:
	void start(const class Tree &tree, class Progress &progress);

protected slots:
	void work();

signals:
	void done(shared_ptr<const class Geometry>);

protected:

	QThread thread;
	const Tree *tree;

public:
	Progress *progress;
};
