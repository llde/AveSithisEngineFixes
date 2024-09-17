#include <Detours\detours.h>
#include <cerrno>
#include <cstdlib>
#include <obse/GameForms.h>
#include <obse/GameObjects.h>
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

void __fastcall LogActor(Actor* This) {
	_MESSAGE("Actor reference %08X  in  %s have null baseForm", This->refID, This->parentCell ? This->parentCell->GetFullName()->name.m_data : "<No Cell>"); 

}

static UInt32 kReturnNull = 0x005E66D2;
static UInt32 kReturnNotNull = 0x005E66A5;
void __declspec(naked) AvoidNullAccess() {
	__asm {
		 mov edi,eax
		 test edi,edi
		 jz null_check
		 jmp [kReturnNotNull]


		 null_check :
			pushad
			call LogActor
			popad
			pop ebx
			jmp[kReturnNull]

	}
}

void  __fastcall  RemoveFormFromSpellListFromEntry(TESSpellList::Entry* list, TESSpellList::Entry* entry){
	ThisStdCall(0x0065C620, list,   entry->type);
}

static UInt32 kReturnTest = 0x0046FCA3;
void  __declspec(naked) FixTESSpellListInfiniteLoad(){
 __asm {
		pushad
		mov edx, ebp
		mov ecx,  esi
		call RemoveFormFromSpellListFromEntry
		popad
		mov  ebp,0
		jmp [kReturnTest]
	}

}

void  __fastcall  RemoveFormFromSpellListFromEntry1(TESSpellList::Entry* list, TESSpellList::Entry* entry){
	//_MESSAGE("OOK % 08X  %08X  %08X %08X", list, edx, list1, list1->type);
	ThisStdCall(0x0065C620, list,   entry->type);
}

static UInt32 kReturnTest1 = 0x0046FDE9 ;
void  __declspec(naked) FixTESSpellListInfiniteLoad1(){
	__asm {
		pushad
		//EDX Is arg, use __fastcall
		mov ecx , edi
		mov edx, ebp
		call RemoveFormFromSpellListFromEntry1
		popad
		mov ebp, 0 //At this point ebp->next should be effectively the null pointer, but better safe then sorry
		jmp [kReturnTest1]
	}

}


void InstallZlibHook() {
	WriteRelJump(0x00742490, (UInt32)&zlib_InflateInitEx);
	WriteRelJump(0x00743970, (UInt32)&zlib_InflateEnd);
	WriteRelJump(0x007425A0, (UInt32)&zlib_Inflate);
}

void InstallAllowRefractionandMSAA() {
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
	WriteRelJump(0x005E669F, (UInt32)&AvoidNullAccess);
	WriteRelJump(0x0046FC94, (UInt32)&FixTESSpellListInfiniteLoad);

	WriteRelJump(0x0046FDDE, (UInt32)&FixTESSpellListInfiniteLoad1);

	if (inst->updateZlib) {
		_MESSAGE("[PATCH] Update Zlib inflate functions.");
		InstallZlibHook();
	}
	else {
		_MESSAGE("[PATCH] Don't update zlib functions");
	}
}
