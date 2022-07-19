namespace Utils
{
	/*Helper class to load from a simple ini file.*/
	class settingsLoader
	{
	private:
		std::shared_ptr<CSimpleIniA> _ini;
		const char* _section;
		int _loadedSettings;
		const char* _settingsFile;

	public:
		settingsLoader(const char* settingsFile)
		{
			_ini = std::make_shared<CSimpleIniA>();
			_ini->LoadFile(settingsFile);
			if (_ini->IsEmpty()) {
				logger::info("Warning: {} is empty.", settingsFile);
			}
			_settingsFile = settingsFile;
		};
		/*Set the active section. Load() will load keys from this section.*/
		void setActiveSection(const char* section)
		{
			_section = section;
		}
		/*Load a boolean value if present.*/
		void load(bool& settingRef, const char* key)
		{
			if (_ini->GetValue(_section, key)) {
				bool val = _ini->GetBoolValue(_section, key);
				settingRef = val;
				_loadedSettings++;
			}
		}
		/*Load a float value if present.*/
		void load(float& settingRef, const char* key)
		{
			if (_ini->GetValue(_section, key)) {
				float val = static_cast<float>(_ini->GetDoubleValue(_section, key));
				settingRef = val;
				_loadedSettings++;
			}
		}
		/*Load an integer value if present.*/
		void load(uint32_t& settingRef, const char* key)
		{
			if (_ini->GetValue(_section, key)) {
				uint32_t val = static_cast<uint32_t>(_ini->GetDoubleValue(_section, key));
				settingRef = val;
				_loadedSettings++;
			}
		}

		void log()
		{
			logger::info("Loaded {} settings from {}", _loadedSettings, _settingsFile);
		}
	};
}

class NoCrimeTeleportRE
{
private:
	static inline bool bVanillaFactionOnly = false;

public:
	class Hook_PlayerPayFine
	{
	public:
		static void install()
		{
			logger::info("Installing payFine hook...");
			REL::Relocation<std::uintptr_t> playerCharacterVtbl{ RE::VTABLE_PlayerCharacter[0] };

			_onPayFine = playerCharacterVtbl.write_vfunc(0xbb, onPayFine);
			logger::info("...done");
		}

	private:
		static void onPayFine(RE::PlayerCharacter* a_this, RE::TESFaction* a_faction, bool a_goToJail, bool a_removeStolenItems)
		{
			bool gotoJail;
			if (bVanillaFactionOnly) {
				std::uint32_t pluginID = a_faction->GetFormID() >> 24;
				if (pluginID <= 0x4) { //is vanilla formID
					gotoJail = false;
				}
				else {
					gotoJail = a_goToJail; //let orginal arg decide
				}
			} else {
				gotoJail = false;
			}
			_onPayFine(a_this, a_faction, gotoJail, a_removeStolenItems);
		}

		static inline REL::Relocation<decltype(onPayFine)> _onPayFine;
	};

	
	static void readSettings()
	{
		Utils::settingsLoader loader("Data\\SKSE\\Plugins\\NoCrimeTeleportRE\\Settings.ini");
		loader.setActiveSection("General");
		loader.load(bVanillaFactionOnly, "bVanillaFactionOnly");
		loader.log();
	}

};


void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
		// Skyrim lifecycle events.
	case SKSE::MessagingInterface::kPostLoad:  // Called after all plugins have finished running SKSEPlugin_Load.
		// It is now safe to do multithreaded operations, or operations against other plugins.
	case SKSE::MessagingInterface::kPostPostLoad:  // Called after all kPostLoad message handlers have run.
	case SKSE::MessagingInterface::kInputLoaded:   // Called when all game data has been found.
		break;
	case SKSE::MessagingInterface::kDataLoaded:  // All ESM/ESL/ESP plugins have loaded, main menu is now active.
		// It is now safe to access form data.
		break;

		// Skyrim game events.
	case SKSE::MessagingInterface::kNewGame:  // Player starts a new game from main menu.
	case SKSE::MessagingInterface::kPreLoadGame:  // Player selected a game to load, but it hasn't loaded yet.
		// Data will be the name of the loaded save.
	case SKSE::MessagingInterface::kPostLoadGame:  // Player's selected save game has finished loading.
		// Data will be a boolean indicating whether the load was successful.
	case SKSE::MessagingInterface::kSaveGame:      // The player has saved a game.
		// Data will be the save name.
	case SKSE::MessagingInterface::kDeleteGame:  // The player deleted a saved game from within the load menu.
		break;
	}
}

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

#ifdef SKYRIM_SUPPORT_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;

	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);

	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) { Sleep(100); }
#endif

	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	SKSE::Init(a_skse);

	auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	//Do stuff when SKSE initializes here:
	NoCrimeTeleportRE::Hook_PlayerPayFine::install();
	NoCrimeTeleportRE::readSettings();


	return true;
}

