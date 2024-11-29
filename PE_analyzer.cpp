//Including headers
#include "windows.h"
#include <iostream>

//Macroses
#define TO_PSTRUCT(TYPE, offset) (TYPE)(pImageBase+(offset))
#define VAR_OF_PSTRUCT(var, TYPE, offset) TYPE var = TO_PSTRUCT(TYPE, offset)

//Function for loading, mapping and viewing PE file
BOOL LoadPeFile(LPCWSTR FilePath, PUCHAR* ppImageBase) {
	HANDLE hFile = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile) {
		std::cout << "Create file error, maybe wrong path!" << std::endl;
		return false;
	}
	HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0, 0, NULL);
	if (NULL == hFileMapping) {
		std::cout << "Mapping error!" << std::endl;
		return false;
	}
	LPVOID p = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (NULL == p) {
		std::cout << "Viewing file error!" << std::endl;
		return false;
	}
	*ppImageBase = (PUCHAR)p;
	return true;
}

//wmain instead of main is VERY important (Unicode symbols in path)
int wmain(int argc, wchar_t* argv[]) {
	if (argc != 2) {
		std::cout << "Need only 1 additional argument: path to PE file (exe or dll)!" << std::endl;
		return -1;
	}
	//Trying to load file
	LPCWSTR g_FilePath = argv[1];
	PUCHAR pImageBase = nullptr;
	if (!LoadPeFile(g_FilePath, &pImageBase)) {
		return -1;
	}
	//Architecture analyzing
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pImageBase;
	VAR_OF_PSTRUCT(pTempPeHeader, PIMAGE_NT_HEADERS, pDosHeader->e_lfanew);
	PUCHAR p = (PUCHAR)(&pTempPeHeader->Signature);
	PIMAGE_SECTION_HEADER pSectionHeader = nullptr;
	switch (pTempPeHeader->FileHeader.Machine) {
	case IMAGE_FILE_MACHINE_I386:
		std::cout << "Architecture: x86" << std::endl;
		pSectionHeader = (PIMAGE_SECTION_HEADER)(((PUCHAR)pTempPeHeader) + sizeof(IMAGE_NT_HEADERS32));
		break;
	case IMAGE_FILE_MACHINE_AMD64:
		std::cout << "Architecture: x64" << std::endl;
		pSectionHeader = (PIMAGE_SECTION_HEADER)(((PUCHAR)pTempPeHeader) + sizeof(IMAGE_NT_HEADERS64));
		break;
	default:
		std::cout << "Architecture: unknown!" << std::endl;
		return -1;
		break;
	}
	//Sections analyzing
	CHAR nmSection[9];
	memset(nmSection, 0, sizeof(nmSection));
	WORD nSections = pTempPeHeader->FileHeader.NumberOfSections;
	std::cout << "Amount of PE sections: " << nSections << std::endl;
	for (int i = 0; i < nSections; i++) {
		memcpy(nmSection, pSectionHeader->Name, 8);
		std::cout << "section #" << i << " " << nmSection << std::endl;
		pSectionHeader++;
	}
	return 0;
}