#include "coffinterfaces.h"
#include "cofffactory.h"
#include <Windows.h>
#include <fstream>
using namespace Sora;

#define OFFSET(stru, memb)  ((DWORD)&((stru*)0)->memb)

int main()
{
    ICoffFactory* fac = GetX86CoffFactory();
    ICoffBuilder* coff = fac->CreateCoffBuilder();
    ISectionBuilder* id2 = fac->CreateSectionBuilder();
    coff->AppendSection(id2);

    id2->SetName(".idata$2");
    id2->SetCharacteristics(SECH_READ | SECH_WRITE | SECH_ALIGN4);
    
    IMAGE_IMPORT_DESCRIPTOR iid;
    ZeroMemory(&iid, sizeof(iid));

    IRelocatableVar* relVar[3];
    relVar[0] = fac->CreateRelocatableVar();
    relVar[0]->Set("dllName", id2, OFFSET(IMAGE_IMPORT_DESCRIPTOR, Name), 0, RVARelocate);
    relVar[1] = fac->CreateRelocatableVar();
    relVar[1]->Set("FirstThunk", id2, OFFSET(IMAGE_IMPORT_DESCRIPTOR, FirstThunk), 4, RVARelocate);
    relVar[2] = fac->CreateRelocatableVar();
    relVar[2]->Set("OriginalFirstThunk", id2, OFFSET(IMAGE_IMPORT_DESCRIPTOR, OriginalFirstThunk), 4, RVARelocate);
    id2->AppendData((PBYTE)&iid, sizeof(iid), relVar, 3);

    ISectionBuilder* id5 = fac->CreateSectionBuilder();
    id5->SetName(".idata$5");
    id5->SetCharacteristics(SECH_READ | SECH_WRITE | SECH_ALIGN4);
    coff->AppendSection(id5);
    DWORD iatval = 0;
    coff->GetSymbolTableBuilder()->AddSymbol(id5, 0, "FirstThunk", SYST_STATIC, 0);
    relVar[0] = fac->CreateRelocatableVar();
    relVar[0]->Set("LookUp_add", id5, 0, 0, RVARelocate);
    id5->AppendData((PBYTE)&iatval, sizeof(DWORD), relVar, 1);
    id5->AppendData((PBYTE)&iatval, sizeof(DWORD), 0, 0);
    coff->GetSymbolTableBuilder()->AddSymbol(id5, 0, "__imp__add@8", SYST_EXTERN, 0);

    ISectionBuilder* id4 = fac->CreateSectionBuilder();
    id4->SetName(".idata$4");
    id4->SetCharacteristics(SECH_READ | SECH_WRITE | SECH_ALIGN4);
    coff->AppendSection(id4);
    relVar[0] = fac->CreateRelocatableVar();
    relVar[0]->Set("LookUp_add", id5, 0, 0, RVARelocate);
    id4->AppendData((PBYTE)&iatval, sizeof(DWORD), relVar, 1);
    id4->AppendData((PBYTE)&iatval, sizeof(DWORD), 0, 0);
    coff->GetSymbolTableBuilder()->AddSymbol(id4, 0, "OriginalFirstThunk", SYST_STATIC, 0);

    char dln[] = "a.dll";
    ISectionBuilder* id6 = fac->CreateSectionBuilder();
    id6->SetName(".idata$6");
    id6->SetCharacteristics(SECH_READ | SECH_WRITE | SECH_ALIGN2);
    coff->AppendSection(id6);
    id6->AppendData((PBYTE)dln, sizeof(dln), 0, 0);
    coff->GetSymbolTableBuilder()->AddSymbol(id6, 0, "dllName", SYST_STATIC, 0);

    char fn[] = "add";
    WORD hint = 0;
    ISectionBuilder* id7 = fac->CreateSectionBuilder();
    id7->SetName(".idata");
    id7->SetCharacteristics(SECH_READ | SECH_WRITE | SECH_ALIGN2);
    coff->AppendSection(id7);
    id7->AppendData((PBYTE)&hint, sizeof(hint), 0, 0);
    id7->AppendData((PBYTE)fn, sizeof(fn), 0, 0);
    coff->GetSymbolTableBuilder()->AddSymbol(id7, 0, "LookUp_add", SYST_STATIC, 0);

    coff->PushRelocs();

    int l = coff->GetDataLength();
    BYTE* p = new BYTE[l];
    coff->GetRawData(p);

    coff->Dispose();

    std::fstream f("im.obj", std::ios::binary | std::ios::out);
    f.write((char*)p, l);
    f.close();

    delete[] p;
}
