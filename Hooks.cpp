#include <Detours\detours.h>
#include <obse/GameForms.h>
#include <obse/NiObjects.h>
#include "Hooks.h"
#include <obse_common/SafeWrite.h>
#include "zlib.h"

#include "Settings.h"

#pragma comment(lib, "zlibstatic.lib")
/*ZLIB HOOKS SECTION*/

int __cdecl zlib_InflateInitEx(z_streamp buffer, const char* versionMode, int size){
	return inflateInit2_(buffer, 15, ZLIB_VERSION, size);
}

int __cdecl zlib_InflateEnd(z_streamp buffer){
	return inflateEnd(buffer);
}

int __cdecl zlib_Inflate(z_streamp buffer, bool flush){
	return  inflate(buffer, flush);
}

/* ZLIB hooks end */


TESForm* (__cdecl* sub_585220)(char*, bool) = (TESForm* (__cdecl* )(char*, bool)) 0x00585220;
TESForm* __cdecl sub_585220Hook(char* Str, bool a2) {
	for (size_t i = 0; i < strlen(Str); i++) {
		if(Str[i] == '/') Str[i]= '\\';
	}
	return 	sub_585220(Str, a2);
}

static constexpr UInt32 kReturn = 0x0047C8F9;

void __declspec(naked) PreventSetLevelCrashBows() {
//TODO better investigate why it crash when it try to dereference the result of dereferencing esi that contain the object, in a way that is NULL
	__asm {
		test eax,eax
		jnz zerocheck  //COunter decrement is 0
		mov edx, [esi]  //Accessing VTBL? Make sense
		test edx,edx    //VTBL NULL, shuld not happen but it is in case of equipped bow. Either that or the Interlocked Decremtn should not be 0
		jz nullcheck //This seems to enter in this code only in case of a Bow equipped. Do This Leak?
		mov  eax, [edx]
		push 1
		mov ecx, esi
		call eax
		jmp [kReturn]

	nullcheck:
	zerocheck:
		jmp [kReturn]
	}
}


void InstallZlibHook() {
	WriteRelJump(0x00742490, (UInt32)&zlib_InflateInitEx);
	WriteRelJump(0x00743970, (UInt32)&zlib_InflateEnd);
	WriteRelJump(0x007425A0, (UInt32)&zlib_Inflate);
}

void InstallAllowRefractionandMSAA() {
	SafeWrite8(0x0040CE11, 0); // Stops to clear the depth buffer when rendering the 1st person node
	WriteNop(0x00498968, 2); // Stops Disabling refraction shaders when MSAA, Non detection path
	WriteRelJump(0x004988D8, 0x0049896A);// Stops Disabling refraction shaders when MSAA, AMD Path
}


#pragma comment(lib, "detours.lib")
void InstallHooks() {
	Settings* inst = Settings::getInstance();
	_MESSAGE("[PATCH] Install Bugfixes");
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&sub_585220, sub_585220Hook); /*Allow prefabs linked with // be packed in BSAs*/
	DetourTransactionCommit();
	WriteRelJump(0x0047C8EB, (UInt32)&PreventSetLevelCrashBows); /*Fix crash when calling SetLevel for Actors with Bows equipped*/
	InstallAllowRefractionandMSAA();
	WriteRelJump(0x0085BD9D, 0x0085BDAD); /*Stub WATER_LAVA 410 pass*/
	WriteRelJump(0x004984BD, 0x004984CD); // Skips antialiasing deactivation if AllowScreenshot is enabled
	if (inst->updateZlib) {
		_MESSAGE("[PATCH] Update Zlib inflate functions.");
		InstallZlibHook();
	}
	else {
		_MESSAGE("[PATCH] Don't update zlib functions");
	}
}