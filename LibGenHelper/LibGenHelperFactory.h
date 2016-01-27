#ifndef LIBGENHELPERFACTORY_H
#define LIBGENHELPERFACTORY_H

#include "LibGenHelperInterfaces.h"

namespace Sora
{
    extern "C" IImportLibraryBuilder* WINAPI CreateX86ImpLibBuilder(LPCSTR szDllName, LPCSTR szMemberName);
    extern "C" IImportLibraryBuilder* WINAPI CreateX64ImpLibBuilder(LPCSTR szDllName, LPCSTR szMemberName);
};

#endif
