#include "LibGenHelperInterfaces.h"
#include "LibGenHelperFactory.h"

#include "LibInterfaces.h"
#include "LibFactory.h"

#include "ImpInterfaces.h"
#include "ImpFactory.h"

#include <string>
#include <vector>

namespace Sora
{
    struct ArchX86 {};
    struct ArchX64 {};

    template<typename Arch>
    struct ArchTraits;

    template<>
    struct ArchTraits<ArchX86>
    {
        static IImpSectionBuilder* GetImpSectionBuilder();
    };

    IImpSectionBuilder* ArchTraits<ArchX86>::GetImpSectionBuilder()
    {
        return GetX86ImpSectionBuilder();
    }

    template<>
    struct ArchTraits<ArchX64>
    {
        static IImpSectionBuilder* GetImpSectionBuilder();
    };

    IImpSectionBuilder* ArchTraits<ArchX64>::GetImpSectionBuilder()
    {
        return GetX64ImpSectionBuilder();
    }

    template<typename Arch>
    class CImportLibraryBuilder : public IImportLibraryBuilder
    {
            IImpSectionBuilder* m_secBuilder;
            ILibraryBuilder* m_libBuilder;
            std::string m_dllName;
            std::string m_memName;

            std::vector<ICoffBuilder*> m_todispose;

            ICoffBuilder* CreateObject()
            {
                ICoffBuilder* r = m_secBuilder->GetCoffFactory()->CreateCoffBuilder();
                m_todispose.push_back(r);
                return r;
            }
        public:
            CImportLibraryBuilder(LPCSTR szDllName, LPCSTR szMemName)
            {
                m_dllName = szDllName;
                m_memName = szMemName;
                m_libBuilder = CreateLibraryBuilder();
                m_secBuilder = ArchTraits<Arch>::GetImpSectionBuilder();

                ICoffBuilder* impdesc = CreateObject();
                m_secBuilder->BuildImportDescriptor(szDllName, impdesc);
                m_libBuilder->AddObject(szMemName, impdesc);

                ICoffBuilder* nuldesc = CreateObject();
                m_secBuilder->BuildNullDescriptor(nuldesc);
                m_libBuilder->AddObject(szMemName, nuldesc);
            }

            void WINAPI Dispose()
            {
                std::vector<ICoffBuilder*>::iterator i, iend;
                i = m_todispose.begin();
                iend = m_todispose.end();
                for(; i != iend; ++i)
                    (*i)->Dispose();
                delete this;
            }

            void WINAPI AddImportFunctionByName(LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szDllExpName)
            {
                ICoffBuilder* impMember = CreateObject();
                m_secBuilder->BuildImportByNameThunk(m_dllName.c_str(), szImpName, szFuncName, szDllExpName, impMember);
                m_libBuilder->AddObject(m_memName.c_str(), impMember);
            }

            void WINAPI AddImportFunctionByOrdinal(LPCSTR szImpName, LPCSTR szFuncName, int nOrdinal)
            {
                ICoffBuilder* impMember = CreateObject();
                m_secBuilder->BuildImportByOrdinalThunk(m_dllName.c_str(), szImpName, szFuncName, nOrdinal, impMember);
                m_libBuilder->AddObject(m_memName.c_str(), impMember);
            }

            void WINAPI AddImportFunctionByNameWithHint(LPCSTR szImpName, LPCSTR szFuncName, LPCSTR szImportName, int nOrdinal)
            {
                ICoffBuilder* impMember = CreateObject();
                m_secBuilder->BuildImportThunk(m_dllName.c_str(), szImpName, szFuncName, szImportName, nOrdinal, impMember);
                m_libBuilder->AddObject(m_memName.c_str(), impMember);
            }

            void WINAPI Build()
            {
                ICoffBuilder* nullThunk = CreateObject();
                m_secBuilder->BuildNullThunk(m_dllName.c_str(), nullThunk);
                m_libBuilder->AddObject(m_memName.c_str(), nullThunk);

                m_libBuilder->FillOffsets();
            }

            void WINAPI GetRawData(PBYTE buf)
            {
                m_libBuilder->GetRawData(buf);
            }

            int WINAPI GetDataLength()
            {
                return m_libBuilder->GetDataLength();
            }
    };

    extern "C" IImportLibraryBuilder* WINAPI CreateX86ImpLibBuilder(LPCSTR szDllName, LPCSTR szMemberName)
    {
        return new CImportLibraryBuilder<ArchX86>(szDllName, szMemberName);
    }

    extern "C" IImportLibraryBuilder* WINAPI CreateX64ImpLibBuilder(LPCSTR szDllName, LPCSTR szMemberName)
    {
        return new CImportLibraryBuilder<ArchX64>(szDllName, szMemberName);
    }
};
