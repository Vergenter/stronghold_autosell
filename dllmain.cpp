// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <algorithm>
#include <iterator>

/**
 * hook destination to source adress
 */
bool Detour32(void* src, void* dst, int len) {
    if (len < 5) return false;

    DWORD curProtection;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &curProtection);

    memset(src, 0x90, len);

    uintptr_t relativeAddress = ((uintptr_t)dst - (uintptr_t)src) - 5;

    *(BYTE*)src = 0xE9;
    *(uintptr_t*)((uintptr_t)src + 1) = relativeAddress;

    VirtualProtect(src, len, curProtection, &curProtection);

    return true;
}
/**
 * revert changes of Detour32
 */
void unDetour(void* src) {
    const int instruction_len = 5;
    DWORD curProtection;
    VirtualProtect(src, instruction_len, PAGE_EXECUTE_READWRITE, &curProtection);
    *(BYTE*)src = 0xE8;
    *(uintptr_t*)((uintptr_t)src + 1) = 0xFFFF5D15;
    VirtualProtect(src, instruction_len, curProtection, &curProtection);
}

typedef int(__thiscall * _Get_sell_value)(void* pThis, int a1, int a2);
_Get_sell_value get_sell_value;

typedef void(__cdecl * _Trade)(int player,int buy0_sell1,int item_type);
_Trade trade;

typedef int(__thiscall * _Print_text)(void* pThis, char* textptr, int text_size, int x, int y, int color, int a7);
_Print_text print_text;


short* const selectionInMarket = (short*)0x115F814;
const unsigned char MATERIAL_IDS[] = {
    2u, // wood
    3u, // hops
    4u, // stone
    6u, // iron
    7u, // pitch
    9u, // wheat
    10u, // bread
    11u, // cheese
    12u, // meat
    13u, // fruit
    14u, // ale
    // 15u, // gold 
    16u, // flour
    // Armory
    17u, // bows
    18u, // crossbows
    19u, // spears
    20u, // pikes
    21u, // maces
    22u, // swords
    23u, // leather armor
    24u // metal armor
};
int comparator_for_MATERIAL_IDS(const unsigned char* lhs, const unsigned char* rhs) {
    if (*lhs < *rhs) return -1;
    else if (*rhs < *lhs) return 1;
    else return 0;
}
const unsigned char* getSelection() {
    return (const unsigned char*)bsearch(selectionInMarket, MATERIAL_IDS, sizeof(MATERIAL_IDS), sizeof(unsigned char), (int(*)(const void*, const void*)) comparator_for_MATERIAL_IDS);
}
/**
 * Every settings value corresponds to material in MATERIAL_IDS
 */
unsigned short defaultSettingsValue = 200;
unsigned short settings[20];

const char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
int* const hooverOverBuy = (int*)0X5FDF0A;
char text[] = "autosell >000 PgUp/PgDn";
/**
 * this for print_text method(__thiscall) defines text drawing style.
 * Points at the same style as style for black bottom layer of sell and buy in market
 */
int* const pthis_print_text = (int*)0x02157824;
void printMyText() {
    // to remove text when hoovering over buy button
    // no need to remove for sell because of place where function is hooked
    if (*hooverOverBuy & 1) return;
    const unsigned char * selection = getSelection();
    if (selection == NULL) return;
    auto index_of_id = selection - MATERIAL_IDS;

    text[10] = digits[(settings[index_of_id] % 1000) / 100];
    text[11] = digits[(settings[index_of_id] % 100) / 10];
    text[12] = digits[settings[index_of_id] % 10];

    // setup arguments for print_text
    *pthis_print_text = 0X57;
    const unsigned x = 0x2E8;
    const unsigned y = 0x420;
    const unsigned color = 0;
    const unsigned unknown = 0;

    print_text(pthis_print_text, text,sizeof(text), x, y, color, unknown);
}

DWORD jumpback;
void __declspec(naked) hookOverGetSellValue() {
    __asm {
        call get_sell_value
        push edx
        push eax
        push ecx
        call printMyText
        pop ecx
        pop eax
        pop edx
        jmp[jumpback]
    }
}

void resetSettings() {
    std::fill(std::begin(settings), std::end(settings), defaultSettingsValue);
}

DWORD jumpbackGameBegin;
void __declspec(naked) hookGameBegin() {
    __asm {
        push eax
        push ebx
        push ecx
        push edx
        push esi
        call resetSettings
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        mov eax, 0x0000190
        jmp[jumpbackGameBegin]
    }
}
/**
 * Order n defined by number of previously built building
 * if 0==n then the market doesn't exist 
 * if n>0 then the market exists
 * if market is destroyed its value is set to 0
 * Value is from line:
 * .text:004570B3 mov     eax, [ecx + ebp + 30EECh]
 * this line is used to get if previous market exists during building new
 * ecx(0x39F4) and ebx(0x0112B0B8) always have same value. Therefore, the eax is always at the same place 0x115F998
 */
unsigned* const MarketBuildOrder = (unsigned* const)0x115F998;
inline bool doesMarketExist() {
    return *MarketBuildOrder > 0;
}
short* const inMenu = (short*)0x1FE7D2C;
const short INTRAIDING = 0x39;

int* const ADDRESS_OF_FIRST_MATERIAL = (int*)0x0115FCC4;
/**
 * Address of every material from MATERIAL_IDS.
 * It's created using ADDRESS_OF_FIRST_MATERIAL(wood) 0x115fcc4 and moving pointer by id difference
 * wood (id=2), stone(id=4) -> difference = 2 -> 0x115fcc4+difference(pointer arithmetic) = 0x115fccc
 * alternative notation: sizeof(int*) = 4 -> 0x115fcc4+difference*sizeof(int*)(int arithmetic) = 0x115fccc
 */
int* const MATERIAL_ADDRESES[] = {
    (int*)0x115fcc4, (int*)0x115fcc8, (int*)0x115fccc, (int*)0x115fcd4,
    (int*)0x115fcd8, (int*)0x115fce0, (int*)0x115fce4, (int*)0x115fce8,
    (int*)0x115fcec, (int*)0x115fcf0, (int*)0x115fcf4, (int*)0x115fcfc,
    (int*)0x115fd00, (int*)0x115fd04, (int*)0x115fd08, (int*)0x115fd0c,
    (int*)0x115fd10, (int*)0x115fd14, (int*)0x115fd18, (int*)0x115fd1c
};

void autoTrade() {
    if (!doesMarketExist()) return;
    if (*inMenu == INTRAIDING) {
        const unsigned char* selection = getSelection();
        if (selection != NULL) {
            size_t index_of_id = selection - MATERIAL_IDS;
            if (GetAsyncKeyState(VK_NEXT) & 1) {
                settings[index_of_id] = max(0, settings[index_of_id] - 10);
            }
            if (GetAsyncKeyState(VK_PRIOR) & 1) {
                settings[index_of_id] = min(999, settings[index_of_id] + 10);
            }
        }

    }
#pragma loop(ivdep)
    for(size_t i = 0;i<sizeof(MATERIAL_IDS);++i)
        if (*MATERIAL_ADDRESES[i] > settings[i])
            trade(1, 1, MATERIAL_IDS[i]);
}


DWORD WINAPI MainThread(LPVOID param) {
    resetSettings();

    uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);
    const unsigned tradeFunctionAddress = 0x0065E60;
    const unsigned printTextFunction = 0x72D60;
    const unsigned sellFunctionValue = 0x5B7F0;

    const int hookLenght = 5;
    DWORD addressOfSellFunctionValueCall = 0x465AD6;
    jumpback = addressOfSellFunctionValueCall + hookLenght;
    
    trade = (_Trade)(moduleBase + tradeFunctionAddress);
    print_text = (_Print_text)(moduleBase + printTextFunction);
    get_sell_value = (_Get_sell_value)(moduleBase + sellFunctionValue);
    Detour32((void*)addressOfSellFunctionValueCall, hookOverGetSellValue, hookLenght);

    const unsigned gameBegin = 0x04F6CF8;
    jumpbackGameBegin = gameBegin + 5;
    Detour32((void*)gameBegin, hookGameBegin, hookLenght);

    while (!(GetAsyncKeyState(VK_END))) {
        autoTrade();
    }

    unDetour((void*)addressOfSellFunctionValueCall);
    FreeLibraryAndExitThread((HMODULE)param, 0);
    return 0;
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, MainThread, hModule, 0, 0);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

