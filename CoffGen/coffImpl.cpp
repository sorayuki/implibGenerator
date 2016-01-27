#include "coffinterfaces.h"
#include "cofffactory.h"

#include <algorithm>
#include <vector>
#include <string>

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
        typedef DWORD32 UIntPtr;
        enum {
            ArchSign = IMAGE_FILE_MACHINE_I386,
            RelRVA = IMAGE_REL_I386_DIR32NB,
            RelVA = IMAGE_REL_I386_DIR32,
            RelVA64 = 0
        };
        static BYTE PadInst;
        static BYTE PadData;
    };
    BYTE ArchTraits<ArchX86>::PadInst = 0x90;
    BYTE ArchTraits<ArchX86>::PadData = 0;

    template<>
    struct ArchTraits<ArchX64>
    {
        typedef DWORD64 UIntPtr;
        enum {
            ArchSign = IMAGE_FILE_MACHINE_AMD64,
            RelRVA = IMAGE_REL_AMD64_ADDR32NB,
            RelVA = IMAGE_REL_AMD64_ADDR32,
            RelVA64 = IMAGE_REL_AMD64_ADDR64
        };
        static BYTE PadInst;
        static BYTE PadData;
    };
    BYTE ArchTraits<ArchX64>::PadInst = 0x90;
    BYTE ArchTraits<ArchX64>::PadData = 0x90;

    template<>
    struct ArchTraits<ArchIA64>
    {
        typedef DWORD64 UIntPtr;
        enum {
            ArchSign = IMAGE_FILE_MACHINE_IA64,
            RelRVA = IMAGE_REL_IA64_DIR32NB,
            RelVA = IMAGE_REL_IA64_DIR32,
            RelVA64 = IMAGE_REL_IA64_DIR64
        };
        static BYTE PadInst;
        static BYTE PadData;
    };
    BYTE ArchTraits<ArchIA64>::PadInst = 0x00;
    BYTE ArchTraits<ArchIA64>::PadData = 0x00;

    template<class T>
    class CDispose : public T {
        protected:
            virtual ~CDispose() = 0 { }
        public:
            void WINAPI Dispose() {
                delete this;
            }
    };

    template<typename Arch>
    class CCoffFactory : public ICoffFactory
    {
        public:
            ICoffBuilder* WINAPI CreateCoffBuilder() { return new CCoffBuilder<Arch>(); }
            ISectionBuilder* WINAPI CreateSectionBuilder() { return new CSectionBuilder<Arch>(); }
            ISymbolTableBuilder* WINAPI CreateSymbolTableBuilder() { return new CSymbolTableBuilder<Arch>(); }
            IStringTableBuilder* WINAPI CreateStringTableBuilder() { return new CStringTableBuilder<Arch>(); }
            IRelocatableVar* WINAPI CreateRelocatableVar() { return new CRelocatableVar<Arch>(); }
            IRelocationTableBuilder* WINAPI CreateRelocationTableBuilder() { return new CRelocationTableBuilder<Arch>(); }

            static CCoffFactory<Arch> Instance;
    };


    template<typename Arch> CCoffFactory<Arch> CCoffFactory<Arch>::Instance;

    extern "C" {
        ICoffFactory* WINAPI GetX86CoffFactory() { return &CCoffFactory<ArchX86>::Instance; }
        ICoffFactory* WINAPI GetX64CoffFactory() { return &CCoffFactory<ArchX64>::Instance; }
        ICoffFactory* WINAPI GetIA64CoffFactory() { return &CCoffFactory<ArchIA64>::Instance; }
    };

    template<typename Arch>
    class CCoffBuilder : public CDispose<ICoffBuilder>
    {
            typedef std::vector<ISectionBuilder*> Sections;
            Sections m_sections;

            ISymbolTableBuilder* m_symbolTable;
            IStringTableBuilder* m_stringTable;
        public:
            int WINAPI AppendSection(ISectionBuilder* p)
            {
                m_sections.push_back(p);
                p->SetSectionIndex(m_sections.size()); //1-based

                return m_sections.size();
            }

            CCoffBuilder()
            {
                m_stringTable = CCoffFactory<Arch>::Instance.CreateStringTableBuilder();
                m_symbolTable = CCoffFactory<Arch>::Instance.CreateSymbolTableBuilder();
                m_symbolTable->SetStringTable(m_stringTable);
            }

            WINAPI ~CCoffBuilder()
            {
                Sections::iterator i, iend;
                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    (*i)->Dispose();
                }

                m_symbolTable->Dispose();
                m_stringTable->Dispose();
            }

            IStringTableBuilder* WINAPI GetStringTableBuilder()
            {
                return m_stringTable;
            }

            ISymbolTableBuilder* WINAPI GetSymbolTableBuilder()
            {
                return m_symbolTable;
            }

            void WINAPI PushRelocs()
            {
                Sections::iterator i, iend;
                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    (*i)->PushRelocs(m_symbolTable);
                }
            }

            int WINAPI GetDataLength()
            {
                Sections::iterator i, iend;
                int len = 0;

                //header
                len += sizeof(IMAGE_FILE_HEADER);

                //sections
                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    len += (*i)->GetHeaderLength();
                    len += (*i)->GetDataLength();
                }

                //symbols
                len += m_symbolTable->GetDataLength();

                //string table
                len += m_stringTable->GetDataLength();

                return len;
            }

            void WINAPI GetRawData(PBYTE p)
            {
                PBYTE sp = p;

                int len = 0;

                //header
                IMAGE_FILE_HEADER* coffHeader = (IMAGE_FILE_HEADER*) p;
                ZeroMemory(coffHeader, sizeof(*coffHeader));
                coffHeader->Machine = ArchTraits<Arch>::ArchSign;
                coffHeader->NumberOfSections = m_sections.size();
                coffHeader->PointerToSymbolTable = 0; //will assign value just before write symbol table
                coffHeader->NumberOfSymbols = m_symbolTable->GetSymbolCount();
                coffHeader->SizeOfOptionalHeader = 0;
                coffHeader->Characteristics = 0;
                p += sizeof(*coffHeader);

                //sections
                Sections::iterator i, iend;

                int secDataBase = p - sp; //current section's offset in file
                //skip section headers
                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    secDataBase += (*i)->GetHeaderLength();
                }

                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    (*i)->GetRawHeader(p, secDataBase);
                    p += (*i)->GetHeaderLength();

                    secDataBase += (*i)->GetDataLength();
                }

                for(i = m_sections.begin(), iend = m_sections.end();
                    i != iend; ++i)
                {
                    (*i)->GetRawData(p);
                    p += (*i)->GetDataLength();
                }

                coffHeader->PointerToSymbolTable = (p - sp);

                //symbols
                m_symbolTable->GetRawData(p);
                p += m_symbolTable->GetDataLength();

                //string table
                m_stringTable->GetRawData(p);
                p += m_stringTable->GetDataLength();
            }
    };

    template<typename Arch>
    class CSectionBuilder : public CDispose<ISectionBuilder>
    {
            DWORD m_chara;
            IRelocationTableBuilder* m_relocTable;

            int m_secIndex;

            std::vector<BYTE> m_buf;
            CHAR m_name[IMAGE_SIZEOF_SHORT_NAME];

            DWORD _outArg;

            int AdjustAlign(DWORD* adjustedSize, DWORD* padSize)
            {
                int as, ps, ras, align;
                ras = m_buf.size();

                if (m_chara & SECH_ALIGN1) align = 1;
                else if (m_chara & SECH_ALIGN2) align = 2;
                else if (m_chara & SECH_ALIGN4) align = 4;
                else if (m_chara & SECH_ALIGN8) align = 8;
                else if (m_chara & SECH_ALIGN16) align = 16;
                else if (m_chara & SECH_ALIGN32) align = 32;
                else if (m_chara & SECH_ALIGN64) align = 64;
                else align = 1;

                if (ras % align == 0) as = ras;
                else as = (ras / align + 1) * align;

                ps = as - ras;

                if (adjustedSize == &_outArg) return as;
                if (padSize == &_outArg) return ps;

                *adjustedSize = as;
                *padSize = ps;

                return 0;
            }

        public:
            CSectionBuilder()
            {
                m_relocTable = CCoffFactory<Arch>::Instance.CreateRelocationTableBuilder();
            }

            WINAPI ~CSectionBuilder()
            {
                m_relocTable->Dispose();
            }

            void WINAPI SetName(LPCSTR p)
            {
                int pl = lstrlenA(p);
                if (pl >= IMAGE_SIZEOF_SHORT_NAME) //no long name support
                    std::copy(p, p + IMAGE_SIZEOF_SHORT_NAME, m_name);
                else
                    lstrcpyA(m_name, p);
            }

            void WINAPI SetSectionIndex(int secIndex)
            {
                m_secIndex = secIndex;
            }

            int WINAPI GetSectionIndex()
            {
                return m_secIndex;
            }

            void WINAPI AppendData(LPCBYTE pBuf, int len, IRelocatableVar* aReloc[], int relocCount)
            {
                int beginPos = m_buf.size();
                m_buf.insert(m_buf.end(), pBuf, pBuf + len);
                for(int i = 0; i < relocCount; ++i) {
                    aReloc[i]->Offset(beginPos);
                    m_relocTable->AppendRelocationItem(aReloc[i]);
                }
            }

            void WINAPI SetCharacteristics(DWORD p)
            {
                m_chara = p;
            }

            int WINAPI GetHeaderLength()
            {
                return sizeof(IMAGE_SECTION_HEADER);
            }

            DWORD WINAPI GetRawCharacteristic(DWORD c)
            {
                return
                      (c & SECH_CODE ? IMAGE_SCN_CNT_CODE : 0)
                    | (c & SECH_READ ? IMAGE_SCN_MEM_READ : 0)
                    | (c & SECH_WRITE ? IMAGE_SCN_MEM_WRITE : 0)
                    | (c & SECH_EXEC ? IMAGE_SCN_MEM_EXECUTE : 0)
                    | (c & SECH_UNINIT ? IMAGE_SCN_CNT_UNINITIALIZED_DATA : (c & SECH_CODE ? 0 : IMAGE_SCN_CNT_INITIALIZED_DATA))
                    | (c & SECH_ALIGN1 ? IMAGE_SCN_ALIGN_1BYTES : 0)
                    | (c & SECH_ALIGN2 ? IMAGE_SCN_ALIGN_2BYTES : 0)
                    | (c & SECH_ALIGN4 ? IMAGE_SCN_ALIGN_4BYTES : 0)
                    | (c & SECH_ALIGN8 ? IMAGE_SCN_ALIGN_8BYTES : 0)
                    | (c & SECH_ALIGN16 ? IMAGE_SCN_ALIGN_16BYTES : 0)
                    | (c & SECH_ALIGN32 ? IMAGE_SCN_ALIGN_32BYTES : 0)
                    | (c & SECH_ALIGN64 ? IMAGE_SCN_ALIGN_64BYTES : 0)
                    | (c & SECH_COMDAT ? IMAGE_SCN_LNK_COMDAT : 0);
            }

            DWORD WINAPI GetRawCharacteristic()
            {
                return GetRawCharacteristic(m_chara);
            }

            void WINAPI GetRawHeader(PBYTE p, DWORD rawOffset)
            {
                IMAGE_SECTION_HEADER* pHeader = (IMAGE_SECTION_HEADER*) p;
                ZeroMemory(pHeader, sizeof(*pHeader));
                std::copy(m_name, m_name+IMAGE_SIZEOF_SHORT_NAME, (LPSTR)pHeader->Name);
                pHeader->Misc.VirtualSize = 0; //must be 0 for obj
                pHeader->VirtualAddress = 0;
                pHeader->SizeOfRawData = AdjustAlign(&_outArg, 0);
                pHeader->PointerToRawData = rawOffset;
                if (m_relocTable->GetCount() == 0)
                    pHeader->PointerToRelocations = 0;
                else
                    pHeader->PointerToRelocations = rawOffset + AdjustAlign(&_outArg, 0);
                pHeader->NumberOfRelocations = m_relocTable->GetCount();
                pHeader->NumberOfLinenumbers = 0; //must be 0
                pHeader->Characteristics = GetRawCharacteristic(m_chara);
            }

            int WINAPI GetDataLength()
            {
                int r = m_buf.size() + m_relocTable->GetDataLength();
                r += AdjustAlign(0, &_outArg);
                return r;
            }

            void WINAPI GetRawData(PBYTE p)
            {
                std::copy(m_buf.begin(), m_buf.end(), p);
                p += m_buf.size();

                BYTE padbyte = 0;
                if (m_chara & SECH_CODE)
                    padbyte = ArchTraits<Arch>::PadInst;
                else
                    padbyte = ArchTraits<Arch>::PadData;

                int padsize = AdjustAlign(0, &_outArg);
                while(padsize --> 0) {
                    *p = padbyte;
                    ++p;
                }

                m_relocTable->GetRawData(p);
            }

            void WINAPI PushRelocs(ISymbolTableBuilder* table)
            {
                //std::string tname(m_name, m_name + 8);
                //table->AddSymbol(this, 0, tname.c_str(), SYST_STATIC, 0);
                m_relocTable->PushToSymbolTable(table);
            }

            class SecAuxSym : public ISectionAuxSymbol
            {
                public:
                    IMAGE_AUX_SYMBOL u;

                    SecAuxSym()
                    {
                        ZeroMemory(&u, sizeof(u));
                    }

                    int WINAPI GetDataLength()
                    {
                        return sizeof(u);
                    }

                    void WINAPI GetRawData(PBYTE p)
                    {
                        *(PIMAGE_AUX_SYMBOL)p = u;
                    }

                    void WINAPI Dispose()
                    {
                        delete this;
                    }
            };

            ISectionAuxSymbol* WINAPI CreateAuxSymbol(ISectionBuilder* associatedSection, SectionComdat selection)
            {
                SecAuxSym* r = new SecAuxSym();
                r->u.Section.Length = GetDataLength();
                r->u.Section.NumberOfRelocations = m_relocTable->GetCount();
                r->u.Section.NumberOfLinenumbers = 0;
                r->u.Section.CheckSum = 0;
                r->u.Section.Number = associatedSection == 0 ? 0 : associatedSection->GetSectionIndex();

                BYTE& val = r->u.Section.Selection;
                switch(selection) {
                case SECO_NODUPLICATE:
                    val = IMAGE_COMDAT_SELECT_NODUPLICATES;
                    break;
                case SECO_ASSOCIATIVE:
                    val = IMAGE_COMDAT_SELECT_ASSOCIATIVE;
                    break;
                case SECO_SELECTANY:
                    val = IMAGE_COMDAT_SELECT_ANY;
                    break;
                case SECO_SELECTLARGEST:
                    val = IMAGE_COMDAT_SELECT_LARGEST;
                    break;
                case SECO_SELECTSAME:
                    val = IMAGE_COMDAT_SELECT_EXACT_MATCH;
                    break;
                case SECO_SELECTSAMESIZE:
                    val = IMAGE_COMDAT_SELECT_SAME_SIZE;
                    break;
                }

                return r;
            }
    };

    template<typename Arch>
    class CStringTableBuilder : public CDispose<IStringTableBuilder>
    {
            std::vector<CHAR> m_buf;
        public:
            int WINAPI AppendString(LPCSTR p)
            {
                int sl = lstrlenA(p);
                int r = m_buf.size() + sizeof(DWORD);
                m_buf.insert(m_buf.end(), p, p + sl + 1);
                return r;
            }

            LPCSTR WINAPI GetString(int offset)
            {
                return &m_buf[offset - sizeof(DWORD)];
            }

            int WINAPI GetDataLength()
            {
                return m_buf.size() + sizeof(DWORD);
            }

            void WINAPI GetRawData(PBYTE p)
            {
                DWORD* pLen = (DWORD*) p;
                *pLen = GetDataLength();
                p += sizeof(*pLen);

                std::copy(m_buf.begin(), m_buf.end(), p);
            }
    };

    template<typename Arch>
    class CSymbolTableBuilder : public CDispose<ISymbolTableBuilder>
    {
            std::vector<IMAGE_SYMBOL> m_buf;
            IStringTableBuilder* m_strTable;
        public:
            void WINAPI SetStringTable(IStringTableBuilder* t)
            {
                m_strTable = t;
            }

            int WINAPI FindSymbol(LPCSTR name)
            {
                int cnt = m_buf.size();
                int i;
                for(i = 0; i < cnt; ++i) {
                    IMAGE_SYMBOL& x = m_buf[i];
                    LPCSTR cn = m_strTable->GetString(x.N.Name.Long);
                    if (lstrcmpA(cn, name) == 0)
                        return i;

                    i += x.NumberOfAuxSymbols;
                }
                return -1;
            }

            int WINAPI AddSymbol(ISectionBuilder* section, int offset, LPCSTR name, StorageType stype, int auxCnt)
            {
                int ind = FindSymbol(name);
                if (ind != -1)
                    return ind;

                IMAGE_SYMBOL s;
                ZeroMemory(&s, sizeof(s));
                s.N.Name.Short = 0;
                s.N.Name.Long = m_strTable->AppendString(name);
                s.Value = offset;
                s.SectionNumber = section != 0 ? section->GetSectionIndex() : 0;
                s.Type = ((stype == SYST_FUNCTION || stype == SYST_STATICFUNCTION) ? (IMAGE_SYM_DTYPE_FUNCTION << 4) | 0 : 0);
                
                switch(stype) {
                case SYST_STATIC: s.StorageClass = IMAGE_SYM_CLASS_STATIC; break;
                case SYST_EXTERN: s.StorageClass = IMAGE_SYM_CLASS_EXTERNAL; break;
                case SYST_FUNCTION: s.StorageClass = IMAGE_SYM_CLASS_EXTERNAL; break;
                case SYST_STATICFUNCTION: s.StorageClass = IMAGE_SYM_CLASS_STATIC; break;
                case SYST_SECTION: s.StorageClass = IMAGE_SYM_CLASS_SECTION; break;
                }

                s.NumberOfAuxSymbols = auxCnt; //currently not supported

                m_buf.push_back(s);

                return m_buf.size() - 1;
            }

            int WINAPI AddAuxData(IHasRawData* d)
            {
                IMAGE_SYMBOL s;
                if (d->GetDataLength() == sizeof(s)) {
                    d->GetRawData((PBYTE)&s);
                    m_buf.push_back(s);
                    return m_buf.size() - 1;
                } else
                    return -1;
            }

            int WINAPI GetSymbolCount()
            {
                return m_buf.size();
            }

            int WINAPI GetDataLength()
            {
                return sizeof(IMAGE_SYMBOL) * GetSymbolCount();
            }

            void WINAPI GetRawData(PBYTE p)
            {
                IMAGE_SYMBOL* pData = (IMAGE_SYMBOL*) p;
                std::copy(m_buf.begin(), m_buf.end(), pData);
            }

            class CSymbolStrings : public ISymbolStrings
            {
                public:
                    int m_cnt;
                    LPCSTR* m_str;

                    ~CSymbolStrings()
                    {
                        if (m_str != 0)
                            delete[] m_str;
                    }

                    void WINAPI Dispose()
                    {
                        delete this;
                    }

                    int WINAPI GetCount()
                    {
                        return m_cnt;
                    }

                    LPCSTR WINAPI GetString(int nIndex)
                    {
                        return m_str[nIndex];
                    }
            };

            ISymbolStrings* GetPublicSymbolNames()
            {
                std::vector<LPCSTR> tmp;
                CSymbolStrings* r = new CSymbolStrings;

                int cnt = m_buf.size();
                int i;
                for(i = 0; i < cnt; ++i) {
                    IMAGE_SYMBOL& x = m_buf[i];
                    if (x.StorageClass == IMAGE_SYM_CLASS_EXTERNAL && x.SectionNumber > 0) {
                        LPCSTR cn = m_strTable->GetString(x.N.Name.Long);
                        tmp.push_back(cn);
                    }

                    i += x.NumberOfAuxSymbols;
                }

                r->m_cnt = tmp.size();
                r->m_str = new LPCSTR[r->m_cnt];
                std::copy(tmp.begin(), tmp.end(), r->m_str);
                return r;
            }
    };

    template<typename Arch>
    class CRelocatableVar : public CDispose<IRelocatableVar>
    {
            int m_val;
            std::string m_symbol;
            DWORD m_relocType;
            ISectionBuilder* m_section;
            int m_size;
        public:
            void WINAPI Offset(int offset)
            {
                m_val += offset;
            }

            void WINAPI Set(LPCSTR symbol, ISectionBuilder* section, DWORD val, int size, DWORD reloctype)
            {
                m_symbol = symbol;
                m_val = val;
                m_size = size;
                m_relocType = reloctype;
                m_section = section;
            }

            void WINAPI Get(LPCSTR* symbol, ISectionBuilder** section, DWORD* val, int* size, DWORD* reloctype)
            {
                *symbol = m_symbol.c_str();
                *val = m_val;
                *size = m_size;
                *reloctype = m_relocType;
                *section = m_section;
            }
    };

    template<typename Arch>
    class CRelocationTableBuilder : public CDispose<IRelocationTableBuilder>
    {
            typedef std::vector<IRelocatableVar*> Relocs;
            Relocs m_relocs;
            std::vector<IMAGE_RELOCATION> m_buf;

        public:
            WINAPI ~CRelocationTableBuilder()
            {
                Relocs::iterator i, iend;
                for(i = m_relocs.begin(), iend = m_relocs.end();
                    i != iend; ++i)
                {
                    (*i)->Dispose();
                }
            }

            int WINAPI GetPtrLength()
            {
                return sizeof( ArchTraits<Arch>::UIntPtr );
            }

            int WINAPI GetCount()
            {
                return m_relocs.size();
            }

            void WINAPI AppendRelocationItem(IRelocatableVar* p)
            {
                m_relocs.push_back(p);
            }

            void WINAPI PushToSymbolTable(ISymbolTableBuilder* table)
            {
                Relocs::iterator i, iend;
                for(i = m_relocs.begin(), iend = m_relocs.end();
                    i != iend; ++i)
                {
                    ISectionBuilder* section;
                    DWORD relocType;
                    LPCSTR symbolName;
                    DWORD val;
                    int symbolValSize;

                    (*i)->Get(&symbolName, &section, &val, &symbolValSize, &relocType);

                    IMAGE_RELOCATION r;
                    ZeroMemory(&r, sizeof(r));
                    r.VirtualAddress = val;
                    r.SymbolTableIndex = table->AddSymbol(0, symbolValSize, symbolName, SYST_EXTERN, 0);

                    r.Type = 
                          (relocType == VARelocate32 ? ArchTraits<Arch>::RelVA : 0)
                        | (relocType == VARelocate64 ? ArchTraits<Arch>::RelVA64 : 0)
                        | (relocType == RVARelocate ? ArchTraits<Arch>::RelRVA : 0);
                    m_buf.push_back(r);
                }

				for (i = m_relocs.begin(), iend = m_relocs.end();
					i != iend; ++i)
				{
					(*i)->Dispose();
				}
				m_relocs.clear();
            }

            int WINAPI GetDataLength()
            {
                return m_buf.size() * sizeof(IMAGE_RELOCATION);
            }

            void WINAPI GetRawData(PBYTE p)
            {
                IMAGE_RELOCATION* pData = (IMAGE_RELOCATION*) p;
                std::copy(m_buf.begin(), m_buf.end(), pData);
            }
    };
};
