#include "ImpInterfaces.h"
#include "ImpFactory.h"

#include "cofffactory.h"

#include <Windows.h>
#include <WinNT.h>

#include <string>

#define OFFSET(stru, memb)  ((DWORD)&((stru*)0)->memb)

namespace Sora
{
    typedef struct {} ArchX86;
    typedef struct {} ArchX64;
    typedef struct {} ArchIA64;

    template<class Arch>
    struct ArchTraits;

    template<>
    struct ArchTraits<ArchX86>
    {
        enum { JmpMemInstLen = 2,
               ImpDescSectionChara = SECH_READ | SECH_WRITE | SECH_ALIGN4,
               ImpThunkSectionChara = SECH_READ | SECH_WRITE | SECH_ALIGN4,
               ImpLookupSectionChara = SECH_READ | SECH_WRITE | SECH_ALIGN2,
               ImpDllNameSectionChara = SECH_READ | SECH_WRITE | SECH_ALIGN2,
               ImpCallStubSectionChara = SECH_READ | SECH_CODE | SECH_EXEC | SECH_ALIGN2
        };
        typedef DWORD32 UIntPtr;

        static UIntPtr OrdinalFlag;
        static BYTE JmpMemInst[2];

        enum { PtrReloc = VARelocate32,
            RvaReloc = RVARelocate
        };

        static ICoffFactory* GetCoffFactory() {
            return GetX86CoffFactory();
        }
    };
    BYTE ArchTraits<ArchX86>::JmpMemInst[2] = {0xff, 0x25};
    ArchTraits<ArchX86>::UIntPtr ArchTraits<ArchX86>::OrdinalFlag = IMAGE_ORDINAL_FLAG32;

    template<>
    struct ArchTraits<ArchX64>
    {
        enum { JmpMemInstLen = 2,
               ImpDescSectionChara = SECH_READ | SECH_WRITE,
               ImpThunkSectionChara = SECH_READ | SECH_WRITE,
               ImpLookupSectionChara = SECH_READ | SECH_WRITE,
               ImpDllNameSectionChara = SECH_READ | SECH_WRITE,
               ImpCallStubSectionChara = SECH_READ | SECH_CODE | SECH_EXEC
        };
        typedef DWORD64 UIntPtr;

        static UIntPtr OrdinalFlag;
        static BYTE JmpMemInst[2];

        enum { PtrReloc = VARelocate64,
            RvaReloc = RVARelocate
        };

        static ICoffFactory* GetCoffFactory() {
            return GetX64CoffFactory();
        }
    };
    BYTE ArchTraits<ArchX64>::JmpMemInst[2] = {0xff, 0x25};
    ArchTraits<ArchX64>::UIntPtr ArchTraits<ArchX64>::OrdinalFlag = IMAGE_ORDINAL_FLAG64;

    template<class Arch>
    class CImpSectionBuilder : public IImpSectionBuilder {
        ICoffFactory* cf;

        CImpSectionBuilder() {
            cf = ArchTraits<Arch>::GetCoffFactory();
        }

        class TmpStr {
            LPSTR m_str;
        public:
            TmpStr() {
                m_str = 0;
            }
            TmpStr(TmpStr& s) {
                m_str = 0;
                *this = s;
            }
            TmpStr(LPCSTR s) {
                m_str = 0;
                *this = s;
            }
            void Clear() {
                if (m_str != 0)
                    delete[] m_str;
                m_str = 0;
            }
            ~TmpStr() {
                Clear();
            }
            TmpStr& operator = (TmpStr& s) {
                if (this == &s) return *this;
                Clear();
                m_str = s.m_str;
                s.m_str = 0;
                return *this;
            }
            TmpStr& operator = (LPCSTR s) {
                Clear();
                int len = lstrlenA(s);
                m_str = new char[len + 1];
                std::copy(s, s + len + 1, m_str);
                return *this;
            }
            operator LPCSTR () const {
                return m_str;
            }
        };

        TmpStr BuildImpDescSymbolName(LPCSTR szDllName)
        {
            std::string r;
            r += "__IMPORT_DESCRITPOR_";
            r += szDllName;
            return TmpStr(r.c_str());
        }

        TmpStr BuildNullThunkName(LPCSTR szDllName)
        {
            std::string r;
            r += '\x7f';
            r += szDllName;
            r += "_NULL_THUNK_DATA";
            return TmpStr(r.c_str());
        }
    public:
        ICoffFactory* GetCoffFactory()
        {
            return cf;
        }

        int GetPtrSize()
        {
            return sizeof(ArchTraits<Arch>::UIntPtr);
        }

        void WINAPI BuildImportDescriptor(LPCSTR szDllName, ICoffBuilder* cb)
        {
            IMAGE_IMPORT_DESCRIPTOR iid;
            ZeroMemory(&iid, sizeof(iid));

            ISectionBuilder* idSection = cf->CreateSectionBuilder();
            cb->AppendSection(idSection);

            idSection->SetName(".idata$2");
            idSection->SetCharacteristics( ArchTraits<Arch>::ImpDescSectionChara );

            IRelocatableVar* relVar[3];
            int i;
            for(i = 0; i < 3; ++i)
                relVar[i] = cf->CreateRelocatableVar();
            relVar[0]->Set(".idata$4", idSection, OFFSET(IMAGE_IMPORT_DESCRIPTOR, OriginalFirstThunk), GetPtrSize(), ArchTraits<Arch>::RvaReloc);
            relVar[1]->Set(".idata$6", idSection, OFFSET(IMAGE_IMPORT_DESCRIPTOR, Name), 0, ArchTraits<Arch>::RvaReloc);
            relVar[2]->Set(".idata$5", idSection, OFFSET(IMAGE_IMPORT_DESCRIPTOR, FirstThunk), GetPtrSize(), ArchTraits<Arch>::RvaReloc);

            idSection->AppendData((LPBYTE)&iid, sizeof(iid), relVar, 3);

            ISectionBuilder* dllnameSection = cf->CreateSectionBuilder();
            cb->AppendSection(dllnameSection);

            dllnameSection->SetName(".idata$6");
            dllnameSection->SetCharacteristics( ArchTraits<Arch>::ImpLookupSectionChara );

            int dllnameLen = lstrlenA(szDllName) + 1;
            dllnameSection->AppendData((PBYTE)szDllName, dllnameLen, 0, 0);

            DWORD schara = idSection->GetRawCharacteristic(SECH_READ | SECH_WRITE);

            cb->GetSymbolTableBuilder()->AddSymbol(idSection, 0, BuildImpDescSymbolName(szDllName), SYST_EXTERN, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(idSection, schara, ".idata$2", SYST_SECTION, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(dllnameSection, 0, ".idata$6", SYST_STATIC, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(0, schara, ".idata$4", SYST_SECTION, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(0, schara, ".idata$5", SYST_SECTION, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(0, 0, "__NULL_IMPORT_DESCRIPTOR", SYST_EXTERN, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(0, 0, BuildNullThunkName(szDllName), SYST_EXTERN, 0);
        }

        void WINAPI BuildImportThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szDllExpName, WORD nDllExpOrdinal, ICoffBuilder* cb)
        {
            std::string lookupSymbol;

            //----------------------------------------------
            //call stub
            if (szFuncName) {
                ISectionBuilder* callstubSec = cf->CreateSectionBuilder();
                cb->AppendSection(callstubSec);
                callstubSec->SetName(".text");
                callstubSec->SetCharacteristics( ArchTraits<Arch>::ImpCallStubSectionChara | SECH_COMDAT );

                callstubSec->AppendData(ArchTraits<Arch>::JmpMemInst, sizeof(ArchTraits<Arch>::JmpMemInst), 0, 0);

                ArchTraits<Arch>::UIntPtr zeroptr = 0;
                IRelocatableVar* funcptr = cf->CreateRelocatableVar();
                funcptr->Set(szImpName, callstubSec, 0, sizeof(zeroptr), ArchTraits<Arch>::PtrReloc);
                callstubSec->AppendData((PBYTE)&zeroptr, sizeof(zeroptr), &funcptr, 1);

                cb->GetSymbolTableBuilder()->AddSymbol(callstubSec, 0, ".text", SYST_STATIC, 1);
                ISectionAuxSymbol* auxsym = callstubSec->CreateAuxSymbol(0, SECO_NODUPLICATE);
                cb->GetSymbolTableBuilder()->AddAuxData(auxsym);
                auxsym->Dispose();

                cb->GetSymbolTableBuilder()->AddSymbol(callstubSec, 0, szFuncName, SYST_FUNCTION, 0);
            }

            //----------------------------------------------
            //thunk, original thunk
            static LPCSTR secName[] = {".idata$5", ".idata$4"};

            //thunk and original thunk, filled with lookup or ordinal
            int i;
            ISectionBuilder *id5, *id4;
            for(i = 0; i < 2; ++i) { //0 = OriginalThunk, 1 = Thunk
                ISectionBuilder* thunkSection = cf->CreateSectionBuilder();
                if (i == 0)
                    id5 = thunkSection;
                else
                    id4 = thunkSection;

                cb->AppendSection(thunkSection);
                thunkSection->SetName(secName[i]);
                thunkSection->SetCharacteristics( ArchTraits<Arch>::ImpThunkSectionChara | SECH_COMDAT );

                typename ArchTraits<Arch>::UIntPtr val;
                if (szDllExpName != 0) {
                    //by name
                    val = 0;
                    IRelocatableVar* rel = cf->CreateRelocatableVar();
                    rel->Set(".idata$6", thunkSection, 0, 0, ArchTraits<Arch>::RvaReloc);
                    thunkSection->AppendData((PBYTE)&val, sizeof(val), &rel, 1);
                } else {
                    //by ordinal
                    val = ArchTraits<Arch>::OrdinalFlag | nDllExpOrdinal;
                    thunkSection->AppendData((PBYTE)&val, sizeof(val), 0, 0);
                }

                if (i == 0) { // .idata$5, thunk
                    cb->GetSymbolTableBuilder()->AddSymbol(thunkSection, 0, secName[i], SYST_STATIC, 1);
                    ISectionAuxSymbol* secauxsym = thunkSection->CreateAuxSymbol(0, SECO_NODUPLICATE);
                    cb->GetSymbolTableBuilder()->AddAuxData(secauxsym);
                    secauxsym->Dispose();
                    cb->GetSymbolTableBuilder()->AddSymbol(thunkSection, 0, szImpName, SYST_EXTERN, 0);
                } else { //.idata$4
                    cb->GetSymbolTableBuilder()->AddSymbol(thunkSection, 0, secName[i], SYST_STATIC, 1);
                    ISectionAuxSymbol* secauxsym = thunkSection->CreateAuxSymbol(id5, SECO_ASSOCIATIVE);
                    cb->GetSymbolTableBuilder()->AddAuxData(secauxsym);
                    secauxsym->Dispose();
                }
            }
            
            //----------------------------------------------
            //import by name
            if (szDllExpName != 0) {
                ISectionBuilder* impSection = cf->CreateSectionBuilder();
                cb->AppendSection(impSection);

                impSection->SetName(".idata$6");
                impSection->SetCharacteristics( ArchTraits<Arch>::ImpLookupSectionChara | SECH_COMDAT );

                WORD hint = nDllExpOrdinal;
                impSection->AppendData((LPBYTE)&hint, sizeof(hint), 0, 0);

                int nameLen = lstrlenA(szDllExpName) + 1;
                impSection->AppendData((LPBYTE)szDllExpName, nameLen, 0, 0);

                cb->GetSymbolTableBuilder()->AddSymbol(impSection, 0, ".idata$6", SYST_STATIC, 1);
                ISectionAuxSymbol* auxsym = impSection->CreateAuxSymbol(id5, SECO_ASSOCIATIVE);
                cb->GetSymbolTableBuilder()->AddAuxData(auxsym);
                auxsym->Dispose();
            }

            //cb->GetSymbolTableBuilder()->AddSymbol(0, sizeof(ArchTraits<Arch>::UIntPtr), "NullThunk", SYST_EXTERN, 0);
            //cb->GetSymbolTableBuilder()->AddSymbol(0, sizeof(ArchTraits<Arch>::UIntPtr), "NullOriginalThunk", SYST_EXTERN, 0);
            cb->GetSymbolTableBuilder()->AddSymbol(0, 0, BuildImpDescSymbolName(szDllName), SYST_EXTERN, 0);
        }

        void WINAPI BuildImportByNameThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szDllExpName, ICoffBuilder* cb)
        {
            BuildImportThunk(szDllName, szImpName, szFuncName, szDllExpName, 0, cb);
        }

        void WINAPI BuildImportByOrdinalThunk(LPCSTR szDllName, LPCSTR szImpName, LPCSTR szFuncName, WORD nDllExpOrdinal, ICoffBuilder* cb)
        {
            BuildImportThunk(szDllName, szImpName, szFuncName, 0, nDllExpOrdinal, cb);
        }

        void WINAPI BuildNullThunk(LPCSTR szDllName, ICoffBuilder* cb)
        {
            ISectionBuilder* ft = cf->CreateSectionBuilder();
            ISectionBuilder* oft = cf->CreateSectionBuilder();
            cb->AppendSection(ft);
            cb->AppendSection(oft);

            ft->SetName(".idata$5");
            ft->SetCharacteristics( ArchTraits<Arch>::ImpThunkSectionChara );
            oft->SetName(".idata$4");
            oft->SetCharacteristics( ArchTraits<Arch>::ImpThunkSectionChara );

            typename ArchTraits<Arch>::UIntPtr zeroptr = 0;

            ft->AppendData((PBYTE)&zeroptr, sizeof(zeroptr), 0, 0);
            oft->AppendData((PBYTE)&zeroptr, sizeof(zeroptr), 0, 0);

            cb->GetSymbolTableBuilder()->AddSymbol(ft, 0, BuildNullThunkName(szDllName), SYST_EXTERN, 0);
            //cb->GetSymbolTableBuilder()->AddSymbol(oft, 0, "NullOriginalThunk", SYST_EXTERN, 0);
        }

        void WINAPI BuildNullDescriptor(ICoffBuilder* cb)
        {
            IMAGE_IMPORT_DESCRIPTOR iid;
            ZeroMemory(&iid, sizeof(iid));

            ISectionBuilder* nullDesc = cf->CreateSectionBuilder();
            cb->AppendSection(nullDesc);
            nullDesc->SetName(".idata$3");
            nullDesc->SetCharacteristics( ArchTraits<Arch>::ImpDescSectionChara );
            nullDesc->AppendData((PBYTE)&iid, sizeof(iid), 0, 0);

            cb->GetSymbolTableBuilder()->AddSymbol(nullDesc, 0, "__NULL_IMPORT_DESCRIPTOR", SYST_EXTERN, 0);
        }

        static CImpSectionBuilder<Arch> Instance;
    };

    template<typename Arch>
    CImpSectionBuilder<Arch> CImpSectionBuilder<Arch>::Instance;

    extern "C" {
        IImpSectionBuilder* WINAPI GetX86ImpSectionBuilder() { return &CImpSectionBuilder<ArchX86>::Instance; }
        IImpSectionBuilder* WINAPI GetX64ImpSectionBuilder() { return &CImpSectionBuilder<ArchX64>::Instance; }
    }
};
