#include "../CoffGen/coffInterfaces.h"
#include "../CoffGen/cofffactory.h"
#include "ImpInterfaces.h"
#include "ImpFactory.h"

using namespace Sora;

bool SaveCoff(LPCTSTR fn, ICoffBuilder* cb)
{
    cb->PushRelocs();

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
    SaveCoff(TEXT("_impdesc.obj"), desc);
    desc->Dispose();

    ICoffBuilder* NullThunk = cf->CreateCoffBuilder();
    isf->BuildNullThunk(dllname, NullThunk);
    SaveCoff(TEXT("_NullThunk.obj"), NullThunk);
    NullThunk->Dispose();

    ICoffBuilder* impadd = cf->CreateCoffBuilder();
    isf->BuildImportByNameThunk(dllname, "__imp__add@8", "_add@8", "add", impadd);
    SaveCoff(TEXT("func1.obj"), impadd);
    impadd->Dispose();

    ICoffBuilder* impsub = cf->CreateCoffBuilder();
    isf->BuildImportByNameThunk(dllname, "__imp__sub@8", "_sub@8", "sub", impsub);
    SaveCoff(TEXT("func2.obj"), impsub);
    impsub->Dispose();

    ICoffBuilder* nuldesc = cf->CreateCoffBuilder();
    isf->BuildNullDescriptor(nuldesc);
    SaveCoff(TEXT("nuldesc.obj"), nuldesc);
    nuldesc->Dispose();

    return 0;
}
