#include "obse/PluginAPI.h"
#include "obse/CommandTable.h"

#if OBLIVION
#include "obse/GameAPI.h"

/*	As of 0020, ExtractArgsEx() and ExtractFormatStringArgs() are no longer directly included in plugin builds.
	They are available instead through the OBSEScriptInterface.
	To make it easier to update plugins to account for this, the following can be used.
	It requires that g_scriptInterface is assigned correctly when the plugin is first loaded.
*/
#define ENABLE_EXTRACT_ARGS_MACROS 1	// #define this as 0 if you prefer not to use this

#if ENABLE_EXTRACT_ARGS_MACROS

OBSEScriptInterface * g_scriptInterface = NULL;	// make sure you assign to this
#define ExtractArgsEx(...) g_scriptInterface->ExtractArgsEx(__VA_ARGS__)
#define ExtractFormatStringArgs(...) g_scriptInterface->ExtractFormatStringArgs(__VA_ARGS__)

#endif

#else
#include "obse_editor/EditorAPI.h"
#endif

#include "obse/ParamInfos.h"
#include "obse/Script.h"
#include "obse/GameObjects.h"
#include "obse/GameTasks.h"
#include "StringVar.h"
#include <string>


// User Definitions
#include <Hooks.h>

IDebugLog		gLog("AveSithisEngineFixes.log");

PluginHandle				g_pluginHandle = kPluginHandle_Invalid;
/*
//	ThisStdCall(0x0053D260);
//0x0053D260
*/
extern "C" {

bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
{
	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "AveSithisEngineFixes";
	info->version = 1;
	_MESSAGE("AveSithis Engine Fixes  Version 1.1");
	// version checks
	if(!obse->isEditor)
	{
		if(obse->obseVersion < 21)
		{
			_ERROR("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, 20);
			return false;
		}

#if OBLIVION
		if(obse->oblivionVersion != OBLIVION_VERSION)
		{
			_ERROR("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
			return false;
		}
#endif

	}
	else
	{
		// no version checks needed for editor
	}

	// version checks pass

	return true;
}

bool OBSEPlugin_Load(const OBSEInterface* obse)
{
	g_pluginHandle = obse->GetPluginHandle();

	if (!obse->isEditor)
	{
		InstallHooks();
	}

	return true;
}
};
