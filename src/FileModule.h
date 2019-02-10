#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <time.h>

#include "module.h"
#include "value.h"
#include "localscope.h"
#include "evalcontext.h"
#include "annotation.h"

class Parameter : public Assignment
{
public:
	explicit Parameter(const Assignment &assignment) : Assignment(assignment) { }
	virtual ~Parameter() { }

	virtual void addAnnotations(AnnotationList *annotations);
	virtual bool hasAnnotations() const;
	virtual const Annotation *annotation(const std::string &name) const;

private:
	AnnotationMap annotations;
};

class FileModule : public AbstractModule
{
public:
	FileModule() : is_handling_dependencies(false) {}
	virtual ~FileModule();

	AbstractNode *evaluate(class Context &ctx) const;

	virtual AbstractNode *instantiate(const class Context *ctx, const class ModuleContext *evalctx) const;
	virtual std::string dump(const std::string &indent, const std::string &name) const;

	void setModulePath(const std::string &path) { this->path = path; }
	const std::string &modulePath() const { return this->path; }
	void registerUse(const std::string path);
	void registerInclude(const std::string &localpath, const std::string &fullpath);
	time_t includesChanged() const;
	time_t handleDependencies();
	bool hasIncludes() const { return !this->includes.empty(); }
	bool usesLibraries() const { return !this->usedlibs.empty(); }
	bool isHandlingDependencies() const { return this->is_handling_dependencies; }

	std::vector<Parameter> getParameters() const;

	LocalScope scope;
	typedef std::unordered_set<std::string> ModuleContainer;
	ModuleContainer usedlibs;
private:
	struct IncludeFile {
		std::string filename;
	};

	time_t include_modified(const IncludeFile &inc) const;

	typedef std::unordered_map<std::string, struct IncludeFile> IncludeContainer;
	IncludeContainer includes;
	bool is_handling_dependencies;
	std::string path;
};
