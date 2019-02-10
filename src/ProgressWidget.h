#pragma once

#include "qtgettext.h"
#include "ui_ProgressWidget.h"
#include <QTime>
#include <QProgressBar>

class ProgressWidget : public QWidget, public Ui::ProgressWidget
{
	Q_OBJECT;
	Q_PROPERTY(bool wasCanceled READ wasCanceled);
	
public:
	ProgressWidget(QWidget *parent = NULL);
	bool wasCanceled() const;
	int elapsedTime() const;

public slots:
	void setIdle(int cpuId);
	void setCpuData(int cpuId, const std::string &description, int value, int maxValue);
	void setRange(int maximum);
	void setValue(int progress);
	int value() const;
	void cancel();

signals:
	void requestShow();

private:
	bool wascanceled;
	QTime starttime;

	std::vector<QProgressBar*> cpuBars;
};
