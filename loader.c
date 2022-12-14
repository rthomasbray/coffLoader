#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_FUNCTION_NAME_SIZE 1024


// find the function address based on name
LPVOID getFunctionAddress(char* functionNameFull) {

	uint16_t functionName_size = strlen(functionNameFull);
	char functionName[MAX_FUNCTION_NAME_SIZE] = {0};

	if (functionName_size > MAX_FUNCTION_NAME_SIZE) {
		functionName_size = MAX_FUNCTION_NAME_SIZE;
	}
	memcpy(functionName, functionNameFull, functionName_size);

	// external functions should start with "__imp_" i.e. --> __imp_User32$MessageBoxA
	char* prefix = "__imp_";
	printf("\t\t[*] Performing relocation for %s\n", functionName);

	// validate function name starts with __imp_ 
	if (strncmp(prefix, functionName, strlen(prefix)) != 0) {
		printf("\t\t\t[-] Expected \"%s\", recieved %s\n", prefix, functionName);
	}

	// TODO: check if internal beacon function

	// __imp_User32$MessageBoxA --> User32 MessageBoxA
	char * localfunc = NULL;
	char * locallib = strtok_s(functionName + strlen(prefix), "$", &localfunc);

	printf("\t\t\t[D]Locallib: %s \n\t\t\t[D]Localfunc: %s\n", locallib, localfunc);
	if (!locallib || !localfunc) {
		printf("\t\t\t[-] Failing! cannot get local lib or local func info (ex: __imp_User32$MessageBoxA)\n");
		exit(1);
	}

	// get handle to library where function is
	HANDLE libHandle = LoadLibraryA(locallib);
	if (!libHandle) {
		printf("\t\t\t[-] Couldn't get library handle");
		exit(1);
	}

	// get address of function within library
	LPVOID functionAddress = GetProcAddress(libHandle, localfunc);
	if (!functionAddress) {
		printf("\t\t\t[-] Couldn't get function address %s",localfunc);
		exit(1);
	}

	printf("\t\t\t[D] Function address: %p\n", functionAddress);

	return functionAddress;
}


// read in coff file contents as bytes
int getFileToBytes (char * filePath, uint8_t ** outBuffer) {

	// open file, get size
	FILE * filePointer = fopen(filePath, "rb");
	if (filePointer == NULL) {
		printf("Failed to open file, check path\n");
		return 0;
	}

	// get file length
	fseek(filePointer, 0, SEEK_END);
	size_t filelen = ftell(filePointer);
	rewind(filePointer);

	//read to array and return
	*outBuffer  = (uint8_t *)malloc(sizeof(uint8_t) * filelen);
	if (*outBuffer == NULL) {
		printf("Failed to allocate buffer\n");
		return 0;
	}

	// read in file bytes
	size_t bytesRead = fread(*outBuffer,1, filelen, filePointer);
	if (bytesRead != filelen) {
		printf("number of bytes read in\n %lld -- %lld -- %x", bytesRead, filelen, *outBuffer[0]);
		return 0;
	}
	
	// close file pointer, return
	fclose(filePointer);

	return 1;
}

// primary funciton
// 1 --> read in file
// 2 --> calculate 
void load(char * filePath) {

	printf("[*] Start\n");

	// read in file
	uint8_t * bytesBuffer = NULL;
	uint8_t result = getFileToBytes(filePath, &bytesBuffer); 
	if (!result) {
		printf("\texiting\n");
		exit(0);
	}

	// get coff and symbol table info
	PIMAGE_FILE_HEADER coffHeader = (PIMAGE_FILE_HEADER)bytesBuffer;
	PIMAGE_SYMBOL symbolEntry = (PIMAGE_SYMBOL)(bytesBuffer + coffHeader->PointerToSymbolTable);


	IMAGE_SECTION_HEADER textSH;
	IMAGE_SECTION_HEADER rDataSH;

	// find .text, .rdata
	PIMAGE_SECTION_HEADER sectionHeader = (PIMAGE_SECTION_HEADER) (bytesBuffer + sizeof(IMAGE_FILE_HEADER));
	for ( int i = 0; i < coffHeader->NumberOfSections; i++) {
		char * currName = (char *)sectionHeader[i].Name;

		printf("\tSection Name --> %s\n", sectionHeader[i].Name);
		if (strcmp(currName, ".text") == 0) {
			 textSH = sectionHeader[i];
		}
		else if (strcmp(currName,".rdata") == 0) {
			rDataSH = sectionHeader[i];
			
		}
	}

	// get text section relocation entry
	PIMAGE_RELOCATION textRelocationEntry = (PIMAGE_RELOCATION)(bytesBuffer + textSH.PointerToRelocations);

	
	// get needed allocated size 
	// text + rdata + all IMAGE_SYM_CLASS_EXTERNAL
	printf("[*] Calculating size\n");
	size_t numExtRelocations = 0;
	for (int i = 0; i < textSH.NumberOfRelocations; i++) {
		printf("\t[D] Symbol number %d of %d\n", i, textSH.NumberOfRelocations);
		PIMAGE_SYMBOL currSymbol = symbolEntry + textRelocationEntry->SymbolTableIndex;

		if (currSymbol->StorageClass == IMAGE_SYM_CLASS_EXTERNAL && currSymbol->SectionNumber == 0) {
			numExtRelocations++;
		}

		textRelocationEntry++;

	}

	// allocate memory (virtual alloc)
	size_t externalFunctionSpace = sizeof(PVOID) * numExtRelocations;
	printf("[*] External function space needed: %lld\n\t [D] Number of relcations: %lld\n", externalFunctionSpace, numExtRelocations);

	// total allocation size = size of text section + size of read only data + external function space
	size_t allocationSize = externalFunctionSpace + textSH.SizeOfRawData;
	
	// allocate memory 

	printf("[*] Allocating memory\n");
	uint8_t* memoryPointer = VirtualAlloc(NULL, allocationSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!memoryPointer) {
		printf("\t[-]Failed to allocate memory\n");
	}
	printf("\t[D]Allocated Size: %llx\n\t\t[D] externalFunctionSpace: %llx -- TS size: %lx -- RD size: %lx\n", allocationSize, externalFunctionSpace, textSH.SizeOfRawData, rDataSH.SizeOfRawData);
	printf("\t[D]Allocated range: %p - %p\n", memoryPointer, memoryPointer+allocationSize);
	
	// copy text section over
	printf("[*]Copying .text\n");
	printf("\t[D] %p --> %p\n", memoryPointer, memoryPointer+textSH.SizeOfRawData);
	memcpy(memoryPointer, bytesBuffer + textSH.PointerToRawData, textSH.SizeOfRawData);

	// reset textRelocation entry to the start
	textRelocationEntry = (PIMAGE_RELOCATION)(bytesBuffer + textSH.PointerToRelocations);

	// number of external function relocations performed
	uint32_t functionMappingCount = 0;

	// relocate static symbols
	printf("[*] Handling .text relocations\n");
	for (int i = 0; i < textSH.NumberOfRelocations; i++) {

		printf("\t[D]Symbol number %d of %d\n", i+1, textSH.NumberOfRelocations);

		// get current symbol
		PIMAGE_SYMBOL currSymbol = symbolEntry + textRelocationEntry->SymbolTableIndex;
		
		// address where function address will be stored after the code in memory
		size_t * functionAddressOffset = (size_t *)(memoryPointer + textSH.SizeOfRawData);

		// get patch location
		uint32_t * patchLocation = (uint32_t *)(memoryPointer - textSH.VirtualAddress + textRelocationEntry->VirtualAddress );
		printf("\t[D] Patch location %lx\n", patchLocation);

		// is external to coff
		if (currSymbol->StorageClass == IMAGE_SYM_CLASS_EXTERNAL && currSymbol->SectionNumber == 0) {

			//  get function name
			size_t stringOffset = currSymbol->N.LongName[1];
			char* functionName = (((char *) (symbolEntry + coffHeader->NumberOfSymbols)) + stringOffset);
			printf("\t\t[D]External Function Name : %s\n", functionName);

			// get function address
			// couple of checks and then loadlibrarya getprocaddress
			LPVOID funcAddress = getFunctionAddress(functionName);

			if(funcAddress) {
				// get memory space allocated to store external address
				*(functionAddressOffset + functionMappingCount) = (uint64_t)funcAddress;

				// calculate the offset between
				uint32_t locationDifference = (uint32_t)(functionAddressOffset + functionMappingCount) - (uint32_t)(patchLocation) - 4;
				
				// coppy differnce
				*(uint32_t *)patchLocation = locationDifference;
			}
			else {
				printf("[-] Failed to get function address\n");
				return;

			}
			functionMappingCount++;
		}
		// is internal
		else {

			IMAGE_SECTION_HEADER targetSection = sectionHeader[currSymbol->SectionNumber - 1];
			printf("\t\t[D] Section %s -- %d\n", targetSection.Name, currSymbol->SectionNumber - 1);

			printf("\t\t[*] Applying local relocation\n");
			uint8_t * start_targetSection = bytesBuffer +  targetSection.PointerToRawData;
			switch (textRelocationEntry->Type){

				case IMAGE_REL_AMD64_REL32: // The 32-bit relative address from the byte following the relocation. 
					// *patchLocation : offset into section where the data we are looking for is
					// start_targetSection: VA of the start of the target section (probably .rdata)
					// currSymbol->Value: additional symbol offset
					// patchLocation: VA of patch location 
					// 4 : accounting for the size of the address that is being overwritten
					printf("\t\t[D]ST_TS: %lx -- CS->VALUE %x -- patchLocation %lx -- *patchLocation %lx\n", start_targetSection, currSymbol->Value, patchLocation, *patchLocation);
					*patchLocation += start_targetSection + currSymbol->Value - (PBYTE)patchLocation - 4; 
					
					break;
				case IMAGE_REL_AMD64_ADDR32NB: // The 32-bit address without an image base (RVA). 
					*patchLocation = *(uint32_t*)(patchLocation + (start_targetSection - (PBYTE)patchLocation - 4)); 
					break;
				case IMAGE_REL_AMD64_ADDR64: // The 64-bit VA of the relocation target. 
					*patchLocation = *(uint32_t*)(*patchLocation + start_targetSection);  
					break;
				default: // all other cases
					break;
				}
		}
		textRelocationEntry++;

	}
	printf("[*] Finished relocations\n");
	// cast to fp and execute
	VOID(*functionPointer)(char * in, unsigned long datalen);


	// main will not always be the first function
	for (int i = 0 ; i < coffHeader->NumberOfSymbols; i ++) {
		printf("%s\n",symbolEntry->N.ShortName);
		if ( strncmp(symbolEntry->N.ShortName, "go", strlen("go")) == 0) {
			printf("FOUNDIT -- > %x\n", symbolEntry->Value);
			functionPointer = (void(*)(char *, unsigned long))(memoryPointer + symbolEntry->Value);
		}
		symbolEntry++;
	}

	//functionPointer = (void(*)(char *, unsigned long))(memoryPointer);

	printf("Attempting to execute memory at %p\n \t[D] First bytes: %x\n", functionPointer, *((uint32_t *) functionPointer));
	char * helllllo = "hello";


	functionPointer(helllllo, 0);
	printf("[+] Returned to loader\n");
}

int main(int argc, char ** argv) {
	load("./testprogram.out");
	return 0;
}
