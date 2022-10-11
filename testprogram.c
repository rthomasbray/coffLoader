// #include <Windows.h>

// #pragma comment(lib, "user32.lib")

// DECLSPEC_IMPORT DWORD WINAPI User32$MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);


#include <windows.h>
#include <stdio.h>
#include <lm.h>
#include <dsgetdc.h>
 
DECLSPEC_IMPORT DWORD WINAPI NETAPI32$DsGetDcNameA(LPVOID, LPVOID, LPVOID, LPVOID, ULONG, LPVOID);
DECLSPEC_IMPORT DWORD WINAPI NETAPI32$NetApiBufferFree(LPVOID);
WINBASEAPI int __cdecl MSVCRT$printf(const char * __restrict__ _Format,...);
DECLSPEC_IMPORT DWORD WINAPI User32$MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);
DECLSPEC_IMPORT VOID WINAPI Kernel32$Sleep(DWORD);
char* TestGlobalString = "This is a global string";
// /* Can't do stuff like "int testvalue;" in a coff file, because it assumes that
//  * the symbol is like any function, so you would need to allocate a section of bss
//  * (without knowing the size of it), and then resolve the symbol to that. So safer
//  * to just not support that */
int testvalue = 0;

void test(void){
    MSVCRT$printf("Test String from test\n");
    MSVCRT$printf("Test String from test1\n");
    testvalue = 1;
}

int test2(void){
    MSVCRT$printf("Test String from test2\n");
    return 0;
}


void go(char * args, unsigned long alen) {
    DWORD dwRet;
    PDOMAIN_CONTROLLER_INFO pdcInfo;
    // BeaconPrintf(1, "This GlobalString \"%s\"\n", TestGlobalString);
    
    Kernel32$Sleep(10000);
    test();
    MSVCRT$printf("Test ValueBack: %d\n", testvalue);
    (void)test2();
    dwRet = NETAPI32$DsGetDcNameA(NULL, NULL, NULL, NULL, 0, &pdcInfo);
    if (ERROR_SUCCESS == dwRet) {
        MSVCRT$printf("%s", pdcInfo->DomainName);
    }
 
    NETAPI32$NetApiBufferFree(pdcInfo);
    MSVCRT$printf("Test1!!!!!!!!!!!!!!!!!!!!");
	User32$MessageBoxA(NULL, "Test1", "Test2", MB_OK);

    
}