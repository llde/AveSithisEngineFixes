#include "Settings.h"
#include <obse/obse/Utilities.h>

Settings::Settings() {
	std::string ob = GetOblivionDirectory();
	ob += file;
	_MESSAGE("[Settings] Load Settings file at %s", ob.c_str());
	this->updateZlib = GetPrivateProfileInt("Main", "UpdateZlib", 0, ob.c_str());
}