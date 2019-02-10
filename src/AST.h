#pragma once

#include <string>

class Location {
public:
	Location(const std::string &file, int firstLine, int firstCol, int lastLine, int lastCol)
		: file(file), first_line(firstLine), first_col(firstCol), last_line(lastLine), last_col(lastCol) {
	}

	const std::string &path() const { return file; }
	std::string getAbsolutePath(const std::string &filename) const;

	int firstLine() const { return first_line; }
	int firstColumn() const { return first_col; }
	int lastLine() const { return last_line; }
	int lastColumn() const { return last_col; }

	static Location NONE;
private:
	std::string file;
	int first_line;
	int first_col;
	int last_line;
	int last_col;
};

class ASTNode
{
public:
  ASTNode(const Location &loc) : loc(loc) {}
	virtual ~ASTNode() {}

	const Location &location() const { return loc; }
	void setLocation(const Location &loc) { this->loc = loc; }

protected:
	Location loc;
};

enum NodeFlags
{
	None = 0,
	Root = 1,
	Highlight = 2,
	Background = 4
};
