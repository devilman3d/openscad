#include "ProgressWidget.h"
#include <boost/thread.hpp>
#include <QTimer>

ProgressWidget::ProgressWidget(QWidget *parent)
	:QWidget(parent)
{
	setupUi(this);

	int numCpus = boost::thread::hardware_concurrency();
	cpuBars.resize(numCpus);
	for (int i = 0; i < numCpus; ++i) {
		auto cpuBar = new QProgressBar(this);
		cpuBars[i] = cpuBar;
		cpuBar->setObjectName(QString::fromStdString("cpuBar" + std::to_string(i)));
		horizontalLayout->insertWidget(i, cpuBar);
	}

	for (int i = 0; i < cpuBars.size(); ++i)
		setIdle(i);
	setValue(0);
	setRange(0);
	this->wascanceled = false;
	this->starttime.start();

	connect(this->stopButton, SIGNAL(clicked()), this, SLOT(cancel()));

	QTimer::singleShot(250, this, SIGNAL(requestShow()));
}

bool ProgressWidget::wasCanceled() const
{
	return this->wascanceled;
}

/*!
	Returns milliseconds since this widget was created
*/
int ProgressWidget::elapsedTime() const
{
	return this->starttime.elapsed();
}

void ProgressWidget::cancel()
{
	this->wascanceled = true;
}

void ProgressWidget::setIdle(int cpuId)
{
	auto cpuBar = this->cpuBars[cpuId];
	cpuBar->setFormat(QString::fromStdString(std::string("#" + std::to_string(cpuId + 1) + " Idle")));
	cpuBar->setValue(0);
	cpuBar->setMaximum(1);
}

void ProgressWidget::setCpuData(int cpuId, const std::string &description, int value, int maxValue)
{
	auto cpuBar = this->cpuBars[cpuId];
	if (maxValue > 0) {
		cpuBar->setFormat(QString::fromStdString(description + ": %p%"));
		cpuBar->setValue(value);
		cpuBar->setMaximum(maxValue);
	}
	else {
		cpuBar->setFormat(QString::fromStdString(description));
		cpuBar->setValue(value);
		cpuBar->setMaximum(1);
	}
}

void ProgressWidget::setRange(int maximum)
{
	this->progressBar->setRange(0, maximum);
}

void ProgressWidget::setValue(int progress)
{
	this->progressBar->setValue(progress);
}

int ProgressWidget::value() const
{
	return this->progressBar->value();
}
