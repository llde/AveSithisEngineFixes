#pragma once
class Settings {
public:
	const std::string file = std::string("Data\\OBSE\\Plugins\\AveSithisEngineFixes.ini");
	Settings();

	static inline Settings* Singleton = nullptr;

	bool updateZlib;
	bool installMagicTrackingLimitRemoval;

	static Settings* getInstance() {
		if (Singleton) return Singleton;
		Singleton = new Settings();
		return Singleton;
	}
};