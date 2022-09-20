#include <Windows.h>

#pragma comment(lib, "user32.lib")

DECLSPEC_IMPORT DWORD WINAPI User32$MessageBoxA(HWND,LPCSTR,LPCSTR,UINT);


int go(int argc, char ** argv){
	User32$MessageBoxA(NULL, "Test1", "Test2", MB_OK);
	return 0;	
}

