#ifndef IMPINTERFACES_H
#define IMPINTERFACES_H

#include "coffInterfaces.h"

namespace Sora {
    class IImpSectionBuilder {
    public:
        virtual ICoffFactory* GetCoffFactory() = 0;

        //the pointer length. 4 for x86 and 8 for x64
        virtual int GetPtrSize() = 0;

        virtual void WINAPI BuildImportDescriptor(LPCSTR szDllName, ICoffBuilder*) = 0;

        //szImpName: __imp__xxx@nn, referred by linker
        //szDllExpName: the name that exported from dll
        //szFuncName: _xxx@nn, used if declared without __declspec(dllimport). will not generated if szStubName is NULL
        //include Thunk, OriginalThunk, importbyname
        virtual void WINAPI BuildImportByNameThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szDllExpName, ICoffBuilder*) = 0;
        virtual void WINAPI BuildImportByOrdinalThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, WORD szDllExpOrdinal, ICoffBuilder*) = 0;

        virtual void WINAPI BuildNullThunk(LPCSTR szDllName, ICoffBuilder*) = 0;
        virtual void WINAPI BuildNullDescriptor(ICoffBuilder*) = 0;

        //import by name, but with [hint of import_by_name] = nDllExpOrdinal
        //when szDllExpName = NULL, it equals BuildImportByOrdinalThunk
        //when nDllExpOrdinal = 0, it equals BuildImportByNameThunk
        //don't let both = 0 (NULL)
        virtual void WINAPI BuildImportThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szDllExpName, WORD nDllExpOrdinal, ICoffBuilder* cb) = 0;
    };
}

#endif
