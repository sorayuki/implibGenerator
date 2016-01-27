#include "ImpLibFix.h"

#include <winnt.h>

#include <algorithm>
#include <string>

namespace Sora {
    class ImpLibRenameImpl {
            PBYTE m_pData;
            PBYTE m_pDataEnd;

            PIMAGE_ARCHIVE_MEMBER_HEADER GetMemberHeader()
            {
                return (PIMAGE_ARCHIVE_MEMBER_HEADER)m_pData;
            }

            bool GetNumberValue(PBYTE pStart, int nLen, DWORD* val)
            {
                int r = 0;
                PBYTE pEnd = pStart + nLen;
                for(;;) {
                    if (pStart >= pEnd)
                        break;
                    if (*pStart == ' ')
                        break;

                    int n = *pStart - '0';
                    if (n <= 9 && n >= 0) {
                        r = r * 10 + n;
                    } else
                        return false;

                    ++pStart;
                }
                *val = r;
                return true;
            }

            bool CheckLeftDataLen(int len)
            {
                if (m_pData + len < m_pDataEnd)
                    return true;
                else
                    return false;
            }

            bool Step1_CheckSign()
            {
                if (!CheckLeftDataLen(IMAGE_ARCHIVE_START_SIZE))
                    return false;
                if (!std::equal(m_pData, m_pData + IMAGE_ARCHIVE_START_SIZE, IMAGE_ARCHIVE_START))
                    return false;

                m_pData += IMAGE_ARCHIVE_START_SIZE;
                return true;
            }

            bool Step2_GetMemberName(std::string& mn)
            {
                if(!CheckLeftDataLen(sizeof(IMAGE_ARCHIVE_MEMBER_HEADER)))
                    return false;
                PIMAGE_ARCHIVE_MEMBER_HEADER pHeader = GetMemberHeader();
                mn = std::string(pHeader->Name, pHeader->Name + sizeof(pHeader->Name));

                return true;
            }

            bool Step3_CheckIfReservedName(std::string& mn)
            {
                if (!mn.empty() && mn[0] != '/')
                    return false;

                return true;
            }
            
            bool Step4_Rename(LPCSTR szNewName)
            {
                PIMAGE_ARCHIVE_MEMBER_HEADER pHeader = GetMemberHeader();
                std::fill(pHeader->Name, pHeader->Name + sizeof(pHeader->Name), ' ');
                int nNewNameLen = lstrlenA(szNewName);
                if (nNewNameLen > sizeof(pHeader->Name))
                    std::copy(szNewName, szNewName + sizeof(pHeader->Name), pHeader->Name);
                else
                    std::copy(szNewName, szNewName + nNewNameLen, pHeader->Name);

                return true;
            }

            bool Step5_MoveNext()
            {
                PIMAGE_ARCHIVE_MEMBER_HEADER pHeader = GetMemberHeader();
                m_pData += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);

                DWORD nSize;
                if (!GetNumberValue(pHeader->Size, sizeof(pHeader->Size), &nSize))
                    return false;

                if (!CheckLeftDataLen(nSize))
                    return false;
                
                m_pData += nSize;
                
                //pad to 2B align
                if (nSize % 2 == 1)
                    ++m_pData;

                return true;
            }

            bool Step6_IsEof()
            {
                if (CheckLeftDataLen(sizeof(IMAGE_ARCHIVE_MEMBER_HEADER)))
                    return false;
                else
                    return true;
            }

        public:
            ImpLibRenameImpl(PBYTE pData, int len)
            {
                m_pData = pData;
                m_pDataEnd = pData + len;
            }

            int DoWork(LPCSTR szNewName)
            {
                int cnt = 0;

                if(!Step1_CheckSign())
                    return -1;

              doNext:
                std::string mn;

                if(!Step2_GetMemberName(mn))
                    return -1;

                if( Step3_CheckIfReservedName(mn) == false )
                {
                    Step4_Rename(szNewName);
                    ++cnt;
                }
                
                if (Step5_MoveNext() == false)
                    return -1;

                if (Step6_IsEof() == false)
                    goto doNext;

                return cnt;
            }
    };

    int WINAPI RenameImpLibObjects(LPCSTR szNewName, PBYTE pData, int nDataLen)
    {
        ImpLibRenameImpl impl(pData, nDataLen);
        return impl.DoWork(szNewName);
    }

    int WINAPI GetMaxNameLength()
    {
        return sizeof(((PIMAGE_ARCHIVE_MEMBER_HEADER)0)->Name);
    }
};
