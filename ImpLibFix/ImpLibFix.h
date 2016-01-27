#ifndef IMPLIBFIX_H
#define IMPLIBFIX_H

#include <Windows.h>

namespace Sora {
    //not include the \0
    extern "C" int WINAPI GetMaxNameLength();

    //return: how many members renamed.
    //first link member and second link member and longname member won't be renamed
    extern "C" int WINAPI RenameImpLibObjects(LPCSTR szNewName, PBYTE pData, int nDataLen);
};

#endif
