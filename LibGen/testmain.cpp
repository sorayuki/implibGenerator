#include "LibInterfaces.h"
#include "LibFactory.h"

#include "../CoffGen/coffInterfaces.h"
#include "../ImpGen/ImpInterfaces.h"
#include "../ImpGen/ImpFactory.h"

using namespace Sora;

bool SaveRawData(LPCTSTR fn, IHasRawData* cb)
{
    HANDLE hFile = CreateFile(fn, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE) {
        int len = cb->GetDataLength();
        BYTE* buf = new BYTE[len];
        cb->GetRawData(buf);

        DWORD olen;
        WriteFile(hFile, buf, len, &olen, 0);
        CloseHandle(hFile);

        return olen == len;
    }
    return false;
}

int main()
{
    IImpSectionBuilder* isf = GetX86ImpSectionBuilder();
    ICoffFactory* cf = isf->GetCoffFactory();

    LPCSTR dllname = "a.dll";

    ICoffBuilder* desc = cf->CreateCoffBuilder();
    isf->BuildImportDescriptor(dllname, desc);
    desc->PushRelocs();

    ICoffBuilder* NullThunk = cf->CreateCoffBuilder();
    isf->BuildNullThunk(dllname, NullThunk);
    NullThunk->PushRelocs();

    ICoffBuilder* impadd = cf->CreateCoffBuilder();
    isf->BuildImportByNameThunk(dllname, "__imp__add@8", "_add@8", "add", impadd);
    impadd->PushRelocs();

    ICoffBuilder* impsub = cf->CreateCoffBuilder();
    isf->BuildImportByNameThunk(dllname, "__imp__sub@8", "_sub@8", "sub", impsub);
    impsub->PushRelocs();

    ICoffBuilder* nuldesc = cf->CreateCoffBuilder();
    isf->BuildNullDescriptor(nuldesc);
    nuldesc->PushRelocs();

    ILibraryBuilder* lib = CreateLibraryBuilder();
    lib->AddObject("a.dll", desc);
    lib->AddObject("a.dll", nuldesc);
    lib->AddObject("a.dll", impadd);
    lib->AddObject("a.dll", impsub);
    lib->AddObject("a.dll", NullThunk);

    lib->FillOffsets();

    SaveRawData(TEXT("as.lib"), lib);

    return 0;
}
