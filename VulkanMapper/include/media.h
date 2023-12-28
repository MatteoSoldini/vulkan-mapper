#pragma once

#include "vm_types.h"
#include <string>

class Media {
private:
	MediaId_t id;
	std::string filePath;

public:
	Media(MediaId_t id, std::string filePath) {
		Media::id = id;
		Media::filePath = filePath;
	}

	virtual ~Media() {};

	MediaId_t getId() const { return id; }
	std::string getFilePath() { return filePath; }
};
