#include "LibInterfaces.h"
#include "LibFactory.h"

#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <exception>

namespace Sora
{
    //return: is limit enough
    static bool StrCpyNoZero(LPSTR to, LPCSTR from, int limit)
    {
        bool r = true;
        int nFromLen = lstrlenA(from);
        if (nFromLen > limit) {
            nFromLen = limit;
            r = false;
        }
        std::copy(from, from + nFromLen, to);
        return r;
    }

    static void BuildMemberHeader(PIMAGE_ARCHIVE_MEMBER_HEADER h, LPCSTR szName, int nSize)
    {
        char tmpBuffer[32];

        std::fill((char*)h, (char*)(h + 1), ' ');

        std::string strName(szName);
        if (strName.size() >= sizeof(h->Name))
            strName[sizeof(h->Name) - 1] = '/';
        else
            strName += '/';
        
        StrCpyNoZero((LPSTR)h->Name, strName.c_str(), sizeof(h->Name));
        h->Date[0] = '0';
        h->Mode[0] = '0';
        StrCpyNoZero((LPSTR)h->EndHeader, IMAGE_ARCHIVE_END, sizeof(h->EndHeader));

        wsprintfA(tmpBuffer, "%d", nSize);
        StrCpyNoZero((LPSTR)h->Size, tmpBuffer, sizeof(h->Size));
    }

    static DWORD32 GetBigEndian(DWORD32 val)
    {
        std::reverse((PBYTE)&val, ((PBYTE)&val) + sizeof(val));
        return val;
    }

    class CBaseLinkMemberBuilder : public IHasRawData
    {
        public:
            struct StringLesser
            {
                bool operator()(const std::string& lhs, const std::string& rhs)
                {
                    return strcmp(lhs.c_str(), rhs.c_str()) < 0;
                }
            };
        protected:
            //slm: Second link Member
            typedef std::map<ICoffBuilder*, int> OffsetCollection;
            OffsetCollection m_offsets;
            typedef std::map<std::string, ICoffBuilder*, StringLesser> SymbolCollection;
            SymbolCollection m_symbols;

            template<typename Iterator>
            int GetIterIndex(Iterator x, Iterator begin)
            {
                int r = 0;
                while( x != begin ) {
                    ++begin;
                    ++r;
                }
                return r;
            }
        public:
            bool WINAPI SetMemberOffset(ICoffBuilder* member, int offset)
            {
                OffsetCollection::iterator x = m_offsets.find(member);
                if (x != m_offsets.end()) {
                    x->second = offset;
                    return true;
                } else
                    return false;
            }

            void WINAPI AppendMember(ICoffBuilder* member)
            {
                m_offsets.insert( std::make_pair(member, 0) );

                ISymbolStrings* sns = member->GetSymbolTableBuilder()->GetPublicSymbolNames();
                int cnt = sns->GetCount();
                int i;
                for(i = 0; i < cnt; ++i)
                    m_symbols.insert( std::make_pair(std::string(sns->GetString(i)), member) );

                sns->Dispose();
            }
    };

    class CFirstLinkMemberBuilder : virtual public CBaseLinkMemberBuilder
    {
            static bool SortOffsetLesser(const std::pair<int, std::string>& a, const std::pair<int, std::string>& b)
            {
                if (a.first < b.first)
                    return true;
                else
                    return false;
            }

        public:
            void WINAPI GetRawData(PBYTE buf)
            {
                PIMAGE_ARCHIVE_MEMBER_HEADER pHeader;
                *(LPVOID*)&pHeader = buf;
                BuildMemberHeader(pHeader, "", CFirstLinkMemberBuilder::GetDataLength() - sizeof(*pHeader));
                ++pHeader;

                PDWORD32 pSymbolCnt = (PDWORD32) pHeader;
                *pSymbolCnt = GetBigEndian(m_symbols.size());
                ++pSymbolCnt;

                // (offset, symbol) pairs
                std::vector< std::pair<int, std::string> > tmp;

                {
                    SymbolCollection::iterator i, iend;
                    i = m_symbols.begin();
                    iend = m_symbols.end();
                    for(; i != iend; ++i) {
                        OffsetCollection::iterator x;
                        x = m_offsets.find(i->second);
                        tmp.push_back( std::make_pair(x->second, i->first) );
                    }

                    std::sort(tmp.begin(), tmp.end(), &CFirstLinkMemberBuilder::SortOffsetLesser);
                }

                PDWORD32 pOffsets = pSymbolCnt;
                {
                    std::vector< std::pair<int, std::string> >::iterator i, iend;
                    i = tmp.begin();
                    iend = tmp.end();
                    for(; i != iend; ++i) {
                        *pOffsets = GetBigEndian(i->first);
                        ++pOffsets;
                    }
                }

                char* pStringTable = (char*)pOffsets;
                {
                    std::vector< std::pair<int, std::string> >::iterator i = tmp.begin(), iend = tmp.end();
                    for(; i != iend; ++i) {
                        int sl = i->second.size();

                        i->second.copy(pStringTable, sl);
                        pStringTable += sl;
                        *pStringTable = 0;
                        ++pStringTable;
                    }
                }
            }

            int WINAPI GetDataLength()
            {
                int r = 0;
                r += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);
                r += 4; //number of symbols
                r += 4 * m_symbols.size(); //array of offsets, one offset for one symbol
                
                //string table
                SymbolCollection::iterator i = m_symbols.begin(), iend = m_symbols.end();
                for(; i != iend; ++i)
                    r += i->first.size() + 1; //null terminated

                return r;
            }
    };

    class CSecondLinkMemberBuilder : virtual public CBaseLinkMemberBuilder
    {
        public:
            void WINAPI GetRawData(PBYTE buf)
            {
                PIMAGE_ARCHIVE_MEMBER_HEADER mh;
                *(LPVOID*)&mh = buf;
                BuildMemberHeader(mh, "", CSecondLinkMemberBuilder::GetDataLength() - sizeof(*mh)); //not include header
                ++mh;

                PDWORD32 pMemberCnt = (PDWORD32) mh;
                *pMemberCnt = m_offsets.size();
                ++pMemberCnt;

                PDWORD32 arrMemberOffset = pMemberCnt;
                std::set<int> tmpOffsets;
                {
                    OffsetCollection::iterator i = m_offsets.begin(), iend = m_offsets.end();
                    for(; i != iend; ++i)
                        tmpOffsets.insert(i->second);
                    
                    std::copy(tmpOffsets.begin(), tmpOffsets.end(), arrMemberOffset);
                    arrMemberOffset += tmpOffsets.size();
                }
                
                PDWORD32 pSymbolCnt = arrMemberOffset;
                *pSymbolCnt = m_symbols.size();
                ++pSymbolCnt;

                PWORD pSymbolBelongOffset = (PWORD) pSymbolCnt;
                {
                    SymbolCollection::iterator i = m_symbols.begin(), iend = m_symbols.end();
                    for(; i != iend; ++i) {
                        ICoffBuilder* c = i->second;
                        OffsetCollection::iterator x = m_offsets.find(c);
                        std::set<int>::iterator y = tmpOffsets.find(x->second);
                        *pSymbolBelongOffset = GetIterIndex(y, tmpOffsets.begin()) + 1; //1-based index
                        ++pSymbolBelongOffset;
                    }
                }

                char* pStringTable = (char*)pSymbolBelongOffset;
                {
                    SymbolCollection::iterator i = m_symbols.begin(), iend = m_symbols.end();
                    for(; i != iend; ++i) {
                        int sl = i->first.size();

                        i->first.copy(pStringTable, sl);
                        pStringTable += sl;
                        *pStringTable = 0;
                        ++pStringTable;
                    }
                }
            }

            int WINAPI GetDataLength()
            {
                int r = 0;
                r += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);
                r += 4; //number of members;
                r += 4 * m_offsets.size(); //offset array
                r += 4; //number of symbols
                r += 2 * m_symbols.size(); //symbol name index array

                //string table
                SymbolCollection::iterator i = m_symbols.begin(), iend = m_symbols.end();
                for(; i != iend; ++i)
                    r += i->first.size() + 1; //null terminated

                return r;
            }
    };

    class CDoubleLinkMemberBuilder : public CFirstLinkMemberBuilder, public CSecondLinkMemberBuilder
    {
        int WINAPI GetDataLength()
        {
            throw std::logic_error("Not clear which to call");
        }

        void WINAPI GetRawData(PBYTE buf)
        {
            throw std::logic_error("Not clear which to call");
        }
    };

    class CLibraryBuilder : public ILibraryBuilder
    {
            typedef std::pair<std::string, ICoffBuilder*> ArchiveMember;
            std::vector<ArchiveMember> m_members;
            CDoubleLinkMemberBuilder m_linkMember;

            ~CLibraryBuilder()
            {
            }

            bool DoPad(int& size)
            {
                if (size % 2 == 1) {
                    ++size;
                    return true;
                } else
                    return false;
            }

            void DoPad(PBYTE& buf, int size)
            {
                if (size % 2 == 1) {
                    *buf = *IMAGE_ARCHIVE_PAD;
                    ++buf;
                }
            }
        public:
            void WINAPI Dispose()
            {
                delete this;
            }

            void WINAPI GetRawData(PBYTE buf)
            {
                PBYTE pBufBegin = buf;

                //sign
                std::copy(IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START + IMAGE_ARCHIVE_START_SIZE, buf);
                buf += IMAGE_ARCHIVE_START_SIZE;

                //first link member
                m_linkMember.CFirstLinkMemberBuilder::GetRawData(buf);
                buf += m_linkMember.CFirstLinkMemberBuilder::GetDataLength();
                DoPad(buf, buf - pBufBegin);

                //second link member
                m_linkMember.CSecondLinkMemberBuilder::GetRawData(buf);
                buf += m_linkMember.CSecondLinkMemberBuilder::GetDataLength();
                DoPad(buf, buf - pBufBegin);

                std::vector<ArchiveMember>::iterator i, iend;
                i = m_members.begin();
                iend = m_members.end();
                for(; i != iend; ++i) {
                    IMAGE_ARCHIVE_MEMBER_HEADER h;
                    BuildMemberHeader(&h, i->first.c_str(), i->second->GetDataLength());
                    *(PIMAGE_ARCHIVE_MEMBER_HEADER)buf = h;
                    buf += sizeof(h);
                    i->second->GetRawData(buf);
                    buf += i->second->GetDataLength();
                    DoPad(buf, buf - pBufBegin);
                }
            }

            int WINAPI GetDataLength()
            {
                return CalcSizeOrFillOffsets(false);
            }

            void WINAPI AddObject(LPCSTR szName, ICoffBuilder* cb)
            {
				cb->PushRelocs();
                m_members.push_back( std::make_pair(std::string(szName), cb) );
                m_linkMember.AppendMember(cb);
            }

            void WINAPI FillOffsets()
            {
                CalcSizeOrFillOffsets(true);
            }

            int WINAPI CalcSizeOrFillOffsets(bool bFillOffset)
            {
                int curPos = 0;

                //sign
                curPos += IMAGE_ARCHIVE_START_SIZE;

                //first link member
                curPos += m_linkMember.CFirstLinkMemberBuilder::GetDataLength();
                DoPad(curPos);

                //second link member
                curPos += m_linkMember.CSecondLinkMemberBuilder::GetDataLength();
                DoPad(curPos);

                std::vector<ArchiveMember>::iterator i, iend;
                i = m_members.begin();
                iend = m_members.end();
                for(; i != iend; ++i) {
                    if (bFillOffset)
                        m_linkMember.SetMemberOffset(i->second, curPos);

                    curPos += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);
                    curPos += i->second->GetDataLength();
                    DoPad(curPos);
                }

                return curPos;
            }
    };

    extern "C" ILibraryBuilder* WINAPI CreateLibraryBuilder()
    {
        return new CLibraryBuilder;
    }
};
