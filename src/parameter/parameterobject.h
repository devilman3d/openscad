#pragma once

#include "value.h"
#include "Assignment.h"
#include "FileModule.h"
#include "expression.h"

#include <QString>

class ParameterObject
{
public:
	typedef enum { UNDEFINED, COMBOBOX, SLIDER, CHECKBOX, TEXT, NUMBER, VECTOR } parameter_type_t;

	ValuePtr value;
	ValuePtr values;
	ValuePtr defaultValue;
	Value::ValueType dvt;
	parameter_type_t target;
	QString description;
	std::string name;
	bool set;
	std::string groupName;
	bool focus;

private:
	Value::ValueType vt;
	void checkVectorWidget();
	
public:
	ParameterObject();
	void setAssignment(Context *context, const Parameter *assignment, const ValuePtr defaultValue);
	void applyParameter(Parameter &assignment);
	bool operator==(const ParameterObject &second);
	
protected:
	int setValue(const ValuePtr defaultValue, const ValuePtr values);
};
