#include <Detours\detours.h>
#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <obse/GameForms.h>
#include <obse/GameObjects.h>
#include <obse/GameData.h>
#include <obse/GameMagicEffects.h>
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

int (__cdecl* PrintError)(const char*, ...) = (int (__cdecl*)(const char*,...)) 0x004A7A60;

void __fastcall LogActor(Actor* This) {
	const char*cellString = This->parentCell ? This->parentCell->GetFullName()->name.m_data : "<No Cell>";
	_MESSAGE("Actor reference %08X  in  %s have null baseForm", This->refID, cellString);
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

void __fastcall LogNullRef(TESObjectREFR* This) {
	TESObjectCELL* parentCell = This->parentCell;
	const char* cellName;
	if(!parentCell) cellName = "<No Cell>";
	else if(!parentCell->GetFullName()  || !This->parentCell->GetFullName()->name.m_data ) cellName = parentCell->GetEditorName();
	else cellName = This->parentCell->GetFullName()->name.m_data ;
	_MESSAGE("Reference %08X  in  %s have null baseForm, encountered on save load", This->refID, cellName);
	PrintError("Reference %08X  in  %s have null baseForm, encountered on save load", This->refID, cellName);
}

static UInt32 kReturnNullLinkRef = 0x004DFABD;
static UInt32 kReturnNotNullLinkRef = 0x004DFA83;

void __declspec(naked) AvoidNullAccessLinkingRef() {
	__asm {
		call edx
		test eax,eax
		jz nullpointer
		movzx   eax, byte ptr [eax+4]
		jmp [kReturnNotNullLinkRef]

	nullpointer :
		pushad
		call LogNullRef
		popad
		jmp [kReturnNullLinkRef]
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


static  UInt32 IsTrespassPackageExtraDataPresent = 0x0041DF60;

bool __fastcall  FixShouldRespawnNullCheckRoutine(TESObjectREFR* This, UInt32 edx){

	TESForm*  baseForm = This->GetBaseForm();

	if(baseForm == nullptr)  return false;  //Patch for original code, it was entering the if even when baseform was null, triggering a nullptr access

	if(baseForm->typeID - kFormType_NPC > 2  || ThisStdCall(IsTrespassPackageExtraDataPresent, &This->baseExtraList))
	{
		UInt32 type = baseForm->typeID;
		if ( type == kFormType_Container )
		{
				return ((TESObjectCONT*)baseForm)->IsRespawning();
		}
		else if ( type - kFormType_NPC <= 1 )
		{
			return ((TESActorBase*)baseForm)->actorBaseData.IsRespawning();
		}
	}
	return false;
}

static UInt32 FixNoProcessActorAccessCont = 0x005F802D;
static UInt32 FixNoProcessActorAccessRetn = 0x005F80BA;

void __declspec(naked) FixNoProcessActorAccess(void) {
	/*
	 In some condition this function can be triggered on Actors that have no process, and this cause  a CTD. What cause this is not fully understood, however it's semi consistent (albeit the reporter reproduction case wasn't straightforward), possibly involving spells and scripts. This report was caused by an hidden creature actor, used as a marker with no AI (or with a single AI package but no Low Process) in Vlija during combats with another companion
	 */
		__asm {
			mov     ecx, [edi+58h]
			test ecx,ecx
			jz skip
			mov     eax, [ecx]
			mov     edx, [eax+8]
			jmp [FixNoProcessActorAccessCont]

		skip:
		//1C stack
			xor eax,eax
			jmp [FixNoProcessActorAccessRetn]
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



UInt32  __fastcall getNumberActivePotion_HOOK(PlayerCharacter* player){
	//UInt32 ret =  ThisStdCall(0x006623D0, player);
	std::vector<MagicItem*> holder = std::vector<MagicItem*>();
 	MagicTarget::EffectNode* playerEffects = player->magicTarget.GetEffectList();
	if(playerEffects){
		for(MagicTarget::EffectNode* current = playerEffects; current; current = current->next){
			if(current->data && current->data->item && current->data->item->GetMagicType() == MagicItem::MagicTypes::kMagicType_AlchemyItem){
				if(current->data->item->list.HasAllEffectHostile()) continue;
				if(holder.empty()){
					holder.push_back(current->data->item);
				}
				else {
					if(std::find(holder.begin(), holder.end(), current->data->item) == holder.end()) {
						holder.push_back(current->data->item);
					}
				}
			}
		}
	}
	//_MESSAGE("%d %d ", ret, holder.size());
	return holder.size();
}

static UInt32 Oblivion_strstr = 0x009840E0;
static UInt32 notFound = 0x0042F922;
static UInt32 found = 0x0042F7F2;
const char* esm = ".esm\0";

void __stdcall TestP(const char* Buf, const char* ext){
	_MESSAGE("Buf : %s", Buf);
	_MESSAGE("EXT : %s", ext);

}
//TODO, avoid duplicate
void __declspec(naked) Hook_BSALoadFromESP(void) {
__asm{
	   test eax , eax
	   jnz ok
	   lea     eax, [esp+458h-20Ch]
	   push    esm
	   push    eax
	   call    Oblivion_strstr
	   add     esp, 8
	   test    eax, eax
	   jnz ok
	   jmp [notFound]
ok:
	   jmp [found]
	}
}
std::string trim(std::string const& s)
{
	auto const first{ s.find_first_not_of(' ') };
	if (first == std::string::npos)
		return {};
	auto const last{ s.find_last_not_of(' ') };
	return s.substr(first, (last - first + 1));
}

void __stdcall  RemoveDuplicates(char* list){
	std::vector<std::string> holder {};
	std::string bsaList = list;
	std::stringstream ss(bsaList);
	std::string sub {};
	UInt32 size = bsaList.size();

	while (getline (ss, sub, ',')){
			std::string trimmed = trim(sub);
			if(std::find(holder.begin(), holder.end(), trimmed) == holder.end()){
				holder.push_back(trimmed);
			}
	}
	std::string newBSAList {};

	for(std::string& bsaEntry : holder){
			newBSAList.append(bsaEntry  + ", ");
	}
	newBSAList = newBSAList.substr(0, newBSAList.size() -2);
//	_MESSAGE("%s", bsaList.c_str());
//	_MESSAGE("%s", newBSAList.c_str());
	memset(list, 0, size );
	strcpy(list, newBSAList.c_str() );
}

static UInt32 returnToTok = 0x0042F938;
void __declspec(naked) Hook_AvoidListDuplicates(void){
	__asm {
		pushad
		push ebp
		call RemoveDuplicates
		popad
		push 0A319FCh
		jmp [returnToTok]
	}
}


void* (__stdcall * originFun)(UInt32 *a1, char *ArgList, char a3, char a4)  = (void* (__stdcall * )(UInt32 *, char *, char , char ) )0x00442890;

void* __stdcall TestPrint(UInt32 *a1, char *ArgList, char a3, char a4){
	_MESSAGE("%s", ArgList);
	return originFun(a1, ArgList,a3,a4);
}

UInt32 __cdecl  TESNPC_LookupBaseId(ModEntry::Data* file, TESNPC* npc){
	// Clean the mod id bit and then add the file idx
	return (npc->refID & 0x00FFFFFF ) | (file->idx << 24 );
}

UInt32 FaceTexture_Retn = 0x005241D1;

void __declspec(naked) Hook_TESNPC_FaceTexture_IDLookup(void){
	__asm {
			push ebp
			push edi
			push ebx
			call TESNPC_LookupBaseId
			pop ebx
			pop edi
			pop ebp
			mov edi, eax
			push ebp
			push edi
			add ebx,1Ch
			jmp [FaceTexture_Retn]
	}
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
	//WriteRelJump(0x004984BD, 0x004984CD); // Skips antialiasing deactivation if AllowScreenshot is enabled. TODO: actually seems not actually needed for me. TEST AGAIN
	WriteRelJump(0x005E669F, (UInt32)&AvoidNullAccess);
	WriteRelJump(0x004DFA7D , (UInt32) &AvoidNullAccessLinkingRef);
	WriteRelJump(0x0046FC94, (UInt32)&FixTESSpellListInfiniteLoad);
	WriteRelJump(0x0046FDDE, (UInt32)&FixTESSpellListInfiniteLoad1);
	WriteRelJump(0x004DC070, (UInt32)&FixShouldRespawnNullCheckRoutine);
	WriteRelJump(0x005F8025, (UInt32)&FixNoProcessActorAccess);
	WriteRelCall(0x00666615, (UInt32)&getNumberActivePotion_HOOK);
	WriteRelJump(0x0052417E, 0x0052418D );   //Allow Face Textures for ESP defined NPCs
	WriteRelJump(0x0042F7EA, (UInt32)&Hook_BSALoadFromESP);
	WriteRelJump(0x0042F933,(UInt32)&Hook_AvoidListDuplicates );
//	WriteRelJump(0x0042F73C, 0x0042F74E);
//	WriteRelCall(0x005241FB , (UInt32)&TestPrint);
	WriteRelJump(0x005241C9, (UInt32)&Hook_TESNPC_FaceTexture_IDLookup);
//New possible bug to check; Feather effect not working on Ability
	if (inst->installMagicTrackingLimitRemoval) {
		WriteRelJump(0x00613CAC, 0x00613CCA);
		_MESSAGE("[PATCH] Install Magic Projectile limit removal");
	}
	if (inst->updateZlib) {
		_MESSAGE("[PATCH] Update Zlib inflate functions.");
		InstallZlibHook();
	}
	else {
		_MESSAGE("[PATCH] Don't update zlib functions");
	}
}

void InstallEditorHooks() {
	_MESSAGE("[PATCH] Install CS Bugfixes");
	/*EXPORT/Read Face Texture for ESP*/
	WriteRelJump(0x004D5E1E, 0x004D5E2D);
	WriteRelJump(0x004D9617, 0x004D9626);
}
