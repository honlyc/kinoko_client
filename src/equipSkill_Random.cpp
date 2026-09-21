#include "pch.h"
#include "hook.h"
#include "wvs/itemslot.h"
#include "wvs/uitooltip.h"
#include "wvs/util.h"
#include "WzLib/IWzCanvas.h"
#include "common/skillinfo.h"
#include <random>

// Display skill info from equip data, checking new structure from wvs/itemslot.h

// Support WZ change: (int) equipSkillDisplayType node under String.wz -> skill.img -> skill_id
// [Bit0] 0 display skill icon, 1 not display
// [Bit1] 0 display current skill level line , 1 not display
// [Bit2] 0 display both skill general desc and desc of current level, 1 only desc of current level
const wchar_t* equipSkillDisplayTypeNode = L"equipSkillDisplayType";

// Support WZ change: (int) quality node under String.wz -> skill.img -> skill_id -> quality -> x, while x represents skill level
// -1 for grey, 0 by default for white, 1 for orange, 2 for blue, 3 for purple, 4 for gold, 5 for green, 6 for red
const wchar_t* equipSkillQualityNode = L"quality";

// If true, Equip Skill icon background will change depending on the quality
const bool enableSkillQualityBackground = true;

// CUIToolTip::GetFontByType
static auto oriCUIToolTipGetFontByType = reinterpret_cast<IWzFont**(__thiscall*)(CUIToolTip*, IWzFont**, int)>(0x00881D40);
// CUIToolTip::DrawTextSepartedLine
static auto oriCUIToolTipDrawTextSepartedLine = reinterpret_cast<int(__thiscall*)(CUIToolTip*, int, int, int, const char*, IWzFontPtr, int, int, IWzFontPtr)>(0x00894A40);
// CUIToolTip::DrawTextItemName
static auto oriCUIToolTipDrawTextItemName = reinterpret_cast<void(__thiscall*)(CUIToolTip*, int, const char*, IWzFontPtr)>(0x0088CA40);
// IWzGr2DLayer::GetCanvas
static auto oriIWzGr2DLayerGetCanvas = reinterpret_cast<IWzCanvas**(__thiscall*)(IWzGr2DLayer*, IWzCanvas**, const Ztl_variant_t*)>(0x0042A350);
// CSkillInfo::GetSkill
static auto oriCSkillInfoGetSkill = reinterpret_cast<SKILLENTRY*(__thiscall*)(CSkillInfo*, int)>(0x006F1BB0);
// SKILLENTRY::GetIconCanvas
static auto oriSKILLENTRYGetIconCanvas = reinterpret_cast<IWzCanvas**(__thiscall*)(SKILLENTRY*, IWzCanvas**)>(0x006EF610);
// CUIToolTip::DrawCanvasIcon
static auto oriCUIToolTipDrawCanvasIcon = reinterpret_cast<void(__thiscall*)(CUIToolTip*, int, int, IWzCanvasPtr)>(0x00882200);

// Construct a EquipSkill right after original constructor of GW_ItemSlotEquip
static auto GW_ItemSlotEquip_Constructor = reinterpret_cast<void(__thiscall*)(GW_ItemSlotEquip*)>(0x004F5FD0);
void __fastcall GW_ItemSlotEquip_Constructor_hook(GW_ItemSlotEquip* pThis) {

    GW_ItemSlotEquip_Constructor(pThis);
    
    void* pEquipSkillAddress = (void*)((uintptr_t)pThis + 0x139);
    new (pEquipSkillAddress) EquipSkill();
}

// Store the calculated tooltip height increasement
DWORD CalcTooltipHeightIncrease_result = 0;

// Help to operate FILETIME
ULONGLONG FileTimeToULONGLONG(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

// FOR TESTING ------------------------------------------------------------------------------------
int nTestDisplayType = 0;
FILETIME ULONGLONGToFileTime(ULONGLONG ull) {
    FILETIME ft;
    *(ULONGLONG*)&ft = ull;
    return ft;
}

// FOR TESTING ------------------------------------------------------------------------------------
FILETIME GenerateRandomFileTime(int max_months) {
    
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULONGLONG ullNow = FileTimeToULONGLONG(ftNow);

    // 2 Months: 61 days * 24 hours/day * 60 min/hour * 60 s/min * 10,000,000 (100ns/s)
    const ULONGLONG days_in_range = 61;
    const ULONGLONG ullMaxInterval = days_in_range * 24 * 60 * 60 * 10000000;

    std::random_device rd;
    std::mt19937_64 generator(rd()); // 使用 64 位 Mersenne Twister 引擎

    std::uniform_int_distribution<ULONGLONG> distribution(0, ullMaxInterval - 1);
    ULONGLONG ullRandomInterval = distribution(generator);
    ULONGLONG ullRandomTargetTime = ullNow + ullRandomInterval;
    return ULONGLONGToFileTime(ullRandomTargetTime);
}
// FOR TESTING ------------------------------------------------------------------------------------


// Calculate the extra height needed for drawing EquipSkill info
int __stdcall CalcTooltipHeightIncrease(CUIToolTip* cuitooltip, EquipSkill* equipskill) {
    

    if (!equipskill || equipskill->nSkillID == 0 || equipskill->nSkillLevel == 0) {
        //return 0; 
        // FOR TESTING ------------------------------------------------------------------------------------
    }

    // FOR TESTING ------------------------------------------------------------------------------------
    std::vector<int> skillpool = {1211004, 1211002, 1211010, 1211009, 1211011, 1211008, 1211006, 1210001, 1321003, 1321000, 1321001, 1320006, 1320005, 1321007, 1321002, 1320009, 1321010, 1320011, 1320008, 5200007, 5201006, 5201005, 5201004, 5201002, 5201001, 5201003, 5200000, 1000006, 1001005, 1001004, 1001003, 4341008, 4341007, 4341006, 4341005, 4341004, 4341003, 4341002, 4340001, 4341000, 2310008, 2311007, 2311006, 2311005, 2311004, 2311003, 2311002, 2311001, 2310000, 14001005, 14001004, 14001003, 14001002, 14000001, 14000000, 22181003, 22181002, 22181001, 22181000, 32120009, 32121008, 32121007, 32121006, 32121005};
    int skillidIndex = std::uniform_int_distribution<int>(0, skillpool.size() - 1)(std::mt19937(std::random_device()()));
    equipskill->nSkillID = skillpool[skillidIndex];
    equipskill->nSkillLevel = std::uniform_int_distribution<int>(1, 1)(std::mt19937(std::random_device()()));
    FILETIME ftRandom = GenerateRandomFileTime(2);
    if (rand() % 2 == 0) {
        equipskill->tDateExpire = ftRandom;
    }      
    // FOR TESTING ------------------------------------------------------------------------------------

    int nHeightSkillName = 35;
    int nHeightExpireDate = 0;
    int nHeightSkillIconAndDesc = 0;
    int nHeightCurrentLevel = 0;
    int nHeightSkillLevelDesc = 0;

    IWzFontPtr fontType = nullptr;
    oriCUIToolTipGetFontByType(cuitooltip, &fontType, 10);
    
    ZXString<wchar_t> zxStringWzUOL = L"String/Skill.img";
    IWzPropertyPtr pStringWz = get_rm()->GetObjectA(Ztl_bstr_t(zxStringWzUOL)).GetUnknown();

    std::wstring skillidPath = (equipskill->nSkillID < 9999 ? L"000" : L"") + std::to_wstring(equipskill->nSkillID);
    IWzPropertyPtr pEquipSkill = pStringWz->item[skillidPath.c_str()].GetUnknown();
    uint32_t nDisplayType = get_int32(pEquipSkill->item[equipSkillDisplayTypeNode], 0);

    // FOR TESTING ------------------------------------------------------------------------------------
    nDisplayType = (rand() % 2 == 0) ? 0 : std ::uniform_int_distribution<int>(-1, 6)(std::mt19937(std::random_device()()));
    nTestDisplayType = nDisplayType;
    // FOR TESTING ------------------------------------------------------------------------------------

    Ztl_variant_t zEquipSkillLevelDesc = pEquipSkill->item[(L"h" + std::to_wstring(equipskill->nSkillLevel)).c_str()];
    Ztl_variant_t zEquipSkillDesc = zEquipSkillLevelDesc;
    if (!(nDisplayType & 4)) {
        zEquipSkillDesc = pEquipSkill->item[L"desc"];
    }
       
    ZXString<wchar_t> zxEquipSkillDesc = get_bstr(zEquipSkillDesc, L"Undefined Skill Desc");   
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, zxEquipSkillDesc, -1, NULL, 0, NULL, NULL);
    std::string sEquipSkillDesc(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, zxEquipSkillDesc, -1, &sEquipSkillDesc[0], size_needed, NULL, NULL);
    
    int tooltipDisplayLeftOffsetX = (nDisplayType & 1) ? 10 : 87;
    nHeightSkillIconAndDesc = oriCUIToolTipDrawTextSepartedLine(cuitooltip, tooltipDisplayLeftOffsetX, 230, 0, sEquipSkillDesc.c_str(), fontType, 0, 0, fontType);

    if (!(nDisplayType & 1)) {
        nHeightSkillIconAndDesc = nHeightSkillIconAndDesc + 3 > 68 ? nHeightSkillIconAndDesc + 3 : 68;
    } else {
        nHeightSkillIconAndDesc = nHeightSkillIconAndDesc + 3;
    }
 
    if ((nDisplayType & 2) == 0 || (nDisplayType & 4) == 0) {
        nHeightCurrentLevel += 15;
    } else {
        nHeightCurrentLevel += 8;
    }

    if ((nDisplayType & 2) == 0) {
        nHeightCurrentLevel += 16;
    }

    if (!(nDisplayType & 4)) {
        ZXString<wchar_t> zxEquipSkillLevelDesc = get_bstr(zEquipSkillLevelDesc, L"Undefined Skill Level Desc");
        size_needed = WideCharToMultiByte(CP_UTF8, 0, zxEquipSkillLevelDesc, -1, NULL, 0, NULL, NULL);
        std::string sEquipSkillLevelDesc(size_needed, 0);
        WideCharToMultiByte(CP_ACP, 0, zxEquipSkillLevelDesc, -1, &sEquipSkillLevelDesc[0], size_needed, NULL, NULL);
        nHeightSkillLevelDesc = oriCUIToolTipDrawTextSepartedLine(cuitooltip, 10, 230, 0, sEquipSkillLevelDesc.c_str(), fontType, 0, 0, fontType) + 5;
    }

    if (equipskill->tDateExpire.dwLowDateTime != 0 || equipskill->tDateExpire.dwHighDateTime != 0) {      
        nHeightExpireDate += 18;
    }

    return nHeightSkillName + nHeightExpireDate + nHeightSkillIconAndDesc + nHeightCurrentLevel + nHeightSkillLevelDesc;
    
}

// Similar to CUIToolTip::GetItemName, return corresponding IWzFontType number based on quality
int getFontTypeByQuality(int quality) {
    switch (quality) {
    case -1:
        return 4;   // GREY
    case 1:
        return 3;   // ORANGE
    case 2:
        return 5;   // BLUE
    case 3:
        return 6;   // PURPLE
    case 4:
        return 2;   // GOLD
    case 5:
        return 8;   // GREEN
    case 6:
        return 9;   // RED
    default:
        break;
    }
    return 1;       // WHITE
}

int getIconBackgroundByQuality(int quality) {
    int fontType = getFontTypeByQuality(quality);
    switch (fontType) {
    case 4:
        return 0xFFBBBBBB;  // GREY
    case 3:
        return 0xFFFF8811;  // ORANGE
    case 5:
        return 0xFF55AAFF;  // BLUE
    case 6:
        return 0xFFCC66FF;  // PURPLE
    case 2:
        return 0xFFFFFF11;  // GOLD
    case 8:
        return 0xFF33FF00;  // GREEN
    case 9:
        return 0xFFFF0077;  // RED
    default:
        break;
    }
    return 0xA0FFFFFF;      // WHITE
}

// Draw EquipSkill info
void __stdcall DrawTooltipEquipSkill(CUIToolTip* cuitooltip, EquipSkill* equipskill, int nTotalHeight) {

    if (!equipskill || equipskill->nSkillID == 0 || equipskill->nSkillLevel == 0) {
        return; 
    }

    int nOffsetY = nTotalHeight - CalcTooltipHeightIncrease_result + 6;

    // Set IWzPropertyPtr
    ZXString<wchar_t> zxStringWzUOL = L"String/Skill.img";
    IWzPropertyPtr pStringWz = get_rm()->GetObjectA(Ztl_bstr_t(zxStringWzUOL)).GetUnknown();
    std::wstring skillidPath = (equipskill->nSkillID < 9999 ? L"000" : L"") + std::to_wstring(equipskill->nSkillID);
    IWzPropertyPtr pEquipSkill = pStringWz->item[skillidPath.c_str()].GetUnknown();
    IWzPropertyPtr pEquipSkillQuality = pEquipSkill->item[equipSkillQualityNode].GetUnknown();

    // Get basic setting
    uint32_t nQuality = !pEquipSkillQuality ? 0 : get_int32(pEquipSkillQuality->item[std::to_wstring(equipskill->nSkillLevel).c_str()], 0);
    uint32_t nDisplayType = get_int32(pEquipSkill->item[equipSkillDisplayTypeNode], 0);

    // FOR TESTING ------------------------------------------------------------------------------------
    nQuality = std::uniform_int_distribution<int>(-1, 6)(std::mt19937(std::random_device()()));
    nDisplayType = nTestDisplayType;
    // FOR TESTING ------------------------------------------------------------------------------------

    // Set IWzFont for various contents
    IWzFontPtr fontTypeName, fontTypeDesc, fontTypeExpire;
    oriCUIToolTipGetFontByType(cuitooltip, &fontTypeName, getFontTypeByQuality(nQuality));
    oriCUIToolTipGetFontByType(cuitooltip, &fontTypeDesc, 10);
    oriCUIToolTipGetFontByType(cuitooltip, &fontTypeExpire, 25);

    // Draw Skill Name
    Ztl_variant_t zEquipSkillName = pEquipSkill->item[L"name"];
    ZXString<wchar_t> zxEquipSkillName = get_bstr(zEquipSkillName, L"Undefined Skill Name");
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, zxEquipSkillName, -1, NULL, 0, NULL, NULL);
    std::string sEquipSkillName(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, zxEquipSkillName, -1, &sEquipSkillName[0], size_needed, NULL, NULL);
    oriCUIToolTipDrawTextItemName(cuitooltip, nOffsetY, sEquipSkillName.c_str(), fontTypeName);
    nOffsetY += 20;

    // Draw Expire Date
    if (equipskill->tDateExpire.dwLowDateTime != 0 || equipskill->tDateExpire.dwHighDateTime != 0) {
        nOffsetY -= 2;
        FILETIME tCurrentTime;
        GetSystemTimeAsFileTime(&tCurrentTime);
        ULONGLONG targetTime = FileTimeToULONGLONG(equipskill->tDateExpire);
        ULONGLONG currentTime = FileTimeToULONGLONG(tCurrentTime);
        ULONGLONG seconds = (targetTime - currentTime) / 10000000;
        ULONGLONG minutes = seconds / 60;
        seconds %= 60;
        ULONGLONG hours = minutes / 60;
        minutes %= 60;
        ULONGLONG days = hours / 24;
        hours %= 24;
        std::string expireResult = "Expire after ";
        if (days > 0) {
            expireResult += std::to_string(days) + " day" + (days > 1 ? "s " : " ");
        }
        if (hours > 0) {
            expireResult += std::to_string(hours) + " hour" + (hours > 1 ? "s " : " ");
        }
        expireResult += std::to_string(minutes) + " min" + (minutes > 1 ? "s" : "");
        //CUIToolTip::DrawTextCenter
        reinterpret_cast<void(__thiscall*)(CUIToolTip*, int, const char*, IWzFontPtr)>(0x0088C360)(cuitooltip, nOffsetY, expireResult.c_str(), fontTypeExpire);
        //oriCUIToolTipDrawTextSepartedLine(cuitooltip, 10, 230, nOffsetY, expireResult.c_str(), fontTypeExpire, 1, 0, fontTypeExpire);
        nOffsetY += 16;
    }
    
    
    // Draw Separated Line below Skill Name
    Ztl_variant_t vIndex((long)0, (unsigned short)3);
    IWzCanvasPtr canvasCUIToolTip = nullptr;
    oriIWzGr2DLayerGetCanvas(cuitooltip->m_pLayer, &canvasCUIToolTip, &vIndex);
    canvasCUIToolTip->raw_DrawRectangle(6, nOffsetY, cuitooltip->m_nWidth - 12, 1, -1);
    nOffsetY += 10;

    // Draw Skill Icon
    if (!(nDisplayType & 1)) {
        Ztl_variant_t empty;
        SKILLENTRY* skillentry = oriCSkillInfoGetSkill(CSkillInfo::GetInstance(), equipskill->nSkillID);
        IWzCanvasPtr canvasSkillIcon = nullptr;
        oriSKILLENTRYGetIconCanvas(skillentry, &canvasSkillIcon);
        if (enableSkillQualityBackground) {
            canvasCUIToolTip->raw_DrawRectangle(10, nOffsetY, 68, 68, getIconBackgroundByQuality(nQuality));
            canvasCUIToolTip->CopyEx(12, nOffsetY + 2, canvasSkillIcon, CA_OVERWRITE, 64, 64, 0, 0, 0, 0, empty);
        } else {
            oriCUIToolTipDrawCanvasIcon(cuitooltip, 10, nOffsetY, canvasSkillIcon);
        }     
    }

    // Draw Skill Desc
    Ztl_variant_t zEquipSkillLevelDesc = pEquipSkill->item[(L"h" + std::to_wstring(equipskill->nSkillLevel)).c_str()];
    Ztl_variant_t zEquipSkillDesc = zEquipSkillLevelDesc;
    if (!(nDisplayType & 4)) {
        // Display both skill desc and skill level desc when [bit3] == 0
        zEquipSkillDesc = pEquipSkill->item[L"desc"];
    }
    ZXString<wchar_t> zxEquipSkillDesc = get_bstr(zEquipSkillDesc, L"Undefined Skill Desc");
    size_needed = WideCharToMultiByte(CP_UTF8, 0, zxEquipSkillDesc, -1, NULL, 0, NULL, NULL);
    std::string sEquipSkillDesc(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, zxEquipSkillDesc, -1, &sEquipSkillDesc[0], size_needed, NULL, NULL);
    int nEquipSkillDescOffsetX = (nDisplayType & 1) ? 10 : 87;
    int nOffset1 = oriCUIToolTipDrawTextSepartedLine(cuitooltip, nEquipSkillDescOffsetX, 230, nOffsetY, sEquipSkillDesc.c_str(), fontTypeDesc, 1, 0, fontTypeDesc);
    if (!(nDisplayType & 1)) {
        nOffsetY += nOffset1 + 3 > 68 ? nOffset1 + 3 : 68;
    } else {
        nOffsetY += nOffset1;
    }

    // Draw Separated Line below Skill Desc
    nOffsetY += 10;
    if ((nDisplayType & 2) == 0 || (nDisplayType & 4) == 0) {
        canvasCUIToolTip->raw_DrawRectangle(6, nOffsetY, cuitooltip->m_nWidth - 12, 1, -1);
        nOffsetY += 5;
    }    
    
    // Draw Current Level
    if (!(nDisplayType & 2)) {
        std::string sCurrentLevel = ("[Current Level: " + std::to_string(equipskill->nSkillLevel) + "]");
        oriCUIToolTipDrawTextSepartedLine(cuitooltip, 10, 230, nOffsetY, sCurrentLevel.c_str(), fontTypeDesc, 1, 0, fontTypeDesc);
        nOffsetY += 16;
    }

    // Draw Skill Level Desc
    if (!(nDisplayType & 4)) {
        ZXString<wchar_t> zxEquipSkillLevelDesc = get_bstr(zEquipSkillLevelDesc, L"Undefined Skill Level Desc");
        size_needed = WideCharToMultiByte(CP_UTF8, 0, zxEquipSkillLevelDesc, -1, NULL, 0, NULL, NULL);
        std::string sEquipSkillLevelDesc(size_needed, 0);
        WideCharToMultiByte(CP_ACP, 0, zxEquipSkillLevelDesc, -1, &sEquipSkillLevelDesc[0], size_needed, NULL, NULL);
        nOffset1 = oriCUIToolTipDrawTextSepartedLine(cuitooltip, 10, 230, nOffsetY, sEquipSkillLevelDesc.c_str(), fontTypeDesc, 1, 0, fontTypeDesc);
        nOffsetY += nOffset1 + 5;
    }

}


// Hook to calculate increased height needed for Setting 
static uintptr_t CalcTooltipHeightIncrease_hook_jmp = 0x008A6660;
static uintptr_t CalcTooltipHeightIncrease_hook_ret = 0x008A6665;
void __declspec(naked) CalcTooltipHeightIncrease_hook() {
    __asm {
        push    0x0A0                   ; original operation
        pushfd
        pushad

        mov     eax, [ esp + 0x124 ]    ; GW_ItemSlotEquip*
        add     eax, 0x139              ; GW_ItemSlotEquip->EquipSkill
        push    eax                     ; (Param2) EquipSkill* equipskill
        push    edi                     ; (Param1) CUIToolTip* cuitooltip
        call    CalcTooltipHeightIncrease
        mov     dword ptr[CalcTooltipHeightIncrease_result], eax

        popad
        popfd
        add     eax, dword ptr[CalcTooltipHeightIncrease_result]    ; Set CUIToolTip->m_nHeight
        jmp     [ CalcTooltipHeightIncrease_hook_ret ]
    }
}

static uintptr_t DrawToolTipEquipSkill_hook_jmp = 0x008A8757;
static uintptr_t DrawToolTipEquipSkill_hook_ret = 0x008A875C;
void __declspec(naked) DrawToolTipEquipSkill_hook() {
    __asm {
        push    0x0DD                   ; original operation
        pushfd
        pushad

        push    edx                     ; (Param3) int nTotalHeight
        mov     eax, [ esp + 0x140 ]    ; GW_ItemSlotEquip*
        add     eax, 0x139              ; GW_ItemSlotEquip->EquipSkill
        push    eax                     ; (Param2) EquipSkill* equipskill
        push    ebp                     ; (Param1) CUIToolTip* cuitooltip
        call    DrawTooltipEquipSkill

        popad
        popfd
        jmp     [ DrawToolTipEquipSkill_hook_ret ]
    }
}

static uintptr_t DrawToolTipEquipSkillWithoutItemDesc_hook_jmp = 0x008A876B;
static uintptr_t DrawToolTipEquipSkillWithoutItemDesc_hook_rtn = 0x008A8772;
void __declspec(naked) DrawToolTipEquipSkillWithoutItemDesc_hook() {
    __asm {
        mov     ebx, [ esp + 0xA8 ]     ; original operation
        pushfd
        pushad

        mov     eax, [ ebp + 0x8 ]
        push    eax                     ; (Param3) int nTotalHeight
        mov     eax, [ esp + 0x124 ]    ; GW_ItemSlotEquip*
        add     eax, 0x139              ; GW_ItemSlotEquip->EquipSkill
        push    eax                     ; (Param2) EquipSkill* equipskill
        push    ebp                     ; (Param1) CUIToolTip* cuitooltip
        call    DrawTooltipEquipSkill

        popad
        popfd
        jmp     [ DrawToolTipEquipSkillWithoutItemDesc_hook_rtn ]
    }
}

void AttachEquipSkill() {

	Patch4(0x004F76CD + 1, sizeof(GW_ItemSlotEquip));		// Constructor of GW_ItemSlotEquip, expand the size
    Patch4(0x005C3B8A + 1, sizeof(GW_ItemSlotEquip));		// Constructor of GW_ItemSlotEquip, expand the size
    ATTACH_HOOK(GW_ItemSlotEquip_Constructor, GW_ItemSlotEquip_Constructor_hook);   // append EquipSkill to ori GW_ItemSlotEquip

    PatchJmp(CalcTooltipHeightIncrease_hook_jmp, reinterpret_cast<uintptr_t>(&CalcTooltipHeightIncrease_hook));
    PatchJmp(DrawToolTipEquipSkill_hook_jmp, reinterpret_cast<uintptr_t>(&DrawToolTipEquipSkill_hook));
    PatchJmp(DrawToolTipEquipSkillWithoutItemDesc_hook_jmp, reinterpret_cast<uintptr_t>(&DrawToolTipEquipSkillWithoutItemDesc_hook));
    PatchNop(DrawToolTipEquipSkillWithoutItemDesc_hook_jmp + 5, DrawToolTipEquipSkillWithoutItemDesc_hook_rtn - 1);

}