#include <Windows.h>
#include <ObjBase.h>
#include <MsXml2.h>

#include <atlbase.h>
#include <atlstr.h>
#include <atlconv.h>

#include "../LibGenHelper/LibGenHelperInterfaces.h"
#include "../LibGenHelper/LibGenHelperFactory.h"

struct MyMsgException
{
    CString fmt;
    CString msg;
    MyMsgException(LPCTSTR p) : msg(p), fmt("%s") {}
    MyMsgException(LPCTSTR p1, LPCTSTR p2) : fmt(p1), msg(p2) {}
};

int _tmain(int argc, TCHAR* argv[])
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    try
    {
        if (argv[1] != 0 && argv[2] != 0 && argv[3] == 0) {
            CComPtr<IXMLDOMDocument2> xdoc;
            if (xdoc.CoCreateInstance(L"Msxml2.DOMDocument.6.0") != S_OK)
                throw MyMsgException(TEXT("Fail to create XML parser object!"));

            CComVariant strFileName;
            strFileName = SysAllocString( CT2CW(argv[1]) );
            VARIANT_BOOL bIsSucc;
            xdoc->load(strFileName, &bIsSucc);
            if (bIsSucc != VARIANT_TRUE)
                throw MyMsgException(TEXT("Fail to load input file %s!"), argv[1]);

            CComPtr<IXMLDOMNode> xDllNameNode;
            CComBSTR strXPath;
            strXPath = L"/ImportLibrary/DllName";
            xdoc->selectSingleNode(strXPath, &xDllNameNode);
            if (xDllNameNode == NULL)
                throw MyMsgException(TEXT("No DllName node!"));

            Sora::IImportLibraryBuilder* impBuilder;
            CComBSTR strDllName;
            xDllNameNode->get_text(&strDllName);
            CW2AEX<> cvtstrDllName(strDllName, CP_ACP);

            CComPtr<IXMLDOMNode> xAmd64Node;
            strXPath = L"/ImportLibrary/ArchAMD64";
            xdoc->selectSingleNode(strXPath, &xAmd64Node);
            if (xAmd64Node == NULL)
                impBuilder = Sora::CreateX86ImpLibBuilder(cvtstrDllName, cvtstrDllName);
            else
                impBuilder = Sora::CreateX64ImpLibBuilder(cvtstrDllName, cvtstrDllName);

            strXPath = L"/ImportLibrary/Import";
            CComPtr<IXMLDOMNodeList> xImportNodes;
            if (xdoc->selectNodes(strXPath, &xImportNodes) != S_OK)
                throw MyMsgException(TEXT("No Import nodes found!"));

            for(;;) {
                CComPtr<IXMLDOMNode> xImportNode;
                if (xImportNodes->nextNode(&xImportNode) != S_OK)
                    break;

                CComPtr<IXMLDOMNode> xLinkNameNode, xStubNameNode, xImportNameNode, xImportOrdinalNode;
                CComBSTR strLinkName, strStubName, strImportName, strImportOrdinal;

                strXPath = L"./LinkName";
                if (xImportNode->selectSingleNode(strXPath, &xLinkNameNode) != S_OK)
                    throw MyMsgException(TEXT("No LinkName Node!"));

                strXPath = L"./StubName";
                xImportNode->selectSingleNode(strXPath, &xStubNameNode);
                //if no stubname node found, will not generate callstub

                xLinkNameNode->get_text(&strLinkName);
                if (xStubNameNode != NULL) xStubNameNode->get_text(&strStubName);

                HRESULT selectImportName;
                HRESULT selectOrdinal;
                strXPath = L"./ImportName";
                selectImportName = xImportNode->selectSingleNode(strXPath, &xImportNameNode);
                strXPath = L"./Ordinal";
                selectOrdinal = xImportNode->selectSingleNode(strXPath, &xImportOrdinalNode);
                int nOrdinal = 0;

                if (selectImportName == S_OK || selectOrdinal == S_OK) {
                    if (selectImportName == S_OK)
                        xImportNameNode->get_text(&strImportName);

                    if (selectOrdinal == S_OK) {
                        xImportOrdinalNode->get_text(&strImportOrdinal);
                        LPCWSTR pszImportOrdinal = strImportOrdinal;

                        while(*pszImportOrdinal != 0) {
                            nOrdinal *= 10;
                            nOrdinal += *pszImportOrdinal - L'0';
                            ++pszImportOrdinal;
                        }
                    }

                    if (selectImportName == S_OK && selectOrdinal == S_OK) {
                        impBuilder->AddImportFunctionByNameWithHint(
                            CW2AEX<>(strLinkName, CP_UTF8),
                            CW2AEX<>(strStubName, CP_UTF8),
                            CW2AEX<>(strImportName, CP_UTF8),
                            nOrdinal
                        );
                    } else if (selectImportName == S_OK) {
                        impBuilder->AddImportFunctionByName(
                            CW2AEX<>(strLinkName, CP_UTF8),
                            CW2AEX<>(strStubName, CP_UTF8),
                            CW2AEX<>(strImportName, CP_UTF8)
                        );
                    } else if (selectOrdinal == S_OK) {
                        impBuilder->AddImportFunctionByOrdinal(
                            CW2AEX<>(strLinkName, CP_UTF8),
                            CW2AEX<>(strStubName, CP_UTF8),
                            nOrdinal
                        );
                    }
                } else {
                    throw MyMsgException(TEXT("No ImportName or Ordinal Node!"));
                }
            }

            //save file
            impBuilder->Build();

            int nFileSize = impBuilder->GetDataLength();
            CHandle hFile( CreateFile(argv[2], GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL) );
            if ( (HANDLE)hFile == INVALID_HANDLE_VALUE )
                throw MyMsgException(TEXT("Fail to create library File %s!"), argv[2]);

            if (SetFilePointer(hFile, nFileSize, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
                if (GetLastError() != 0)
                    throw MyMsgException(TEXT("Can't allocate disk space for output file!"));

            if (SetEndOfFile(hFile) == FALSE)
                throw MyMsgException(TEXT("Can't allocate disk space for output file!"));

            CHandle hFileMap( CreateFileMapping(hFile, 0, PAGE_READWRITE, 0, nFileSize, 0) );
            if ((HANDLE)hFileMap == NULL)
                throw MyMsgException(TEXT("Can't map output file for writing!"));

            LPVOID pFile = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 0);
            if (pFile == 0)
                throw MyMsgException(TEXT("Can't map output file for writing!"));

            impBuilder->GetRawData((PBYTE)pFile);
            impBuilder->Dispose();
            UnmapViewOfFile(pFile);
        } else {
            fprintf(stdout, "%s",
                "Make import library from XML\n"
                "using: MakeImpLib <input xml> <output lib>\n"
                "\n"
                "XML Sample\n"
                "<ImportLibrary>\n"
                "  <!-- <ArchAMD64 /> -->\n"
                "  <!-- If no ArchAMD64 node, it creates i386 import library -->\n"
                "  <DllName>Kernel32.dll</DllName>\n"
                "  <Import>\n"
                "    <LinkName>__imp__SleepEx@8</LinkName>\n"
                "    <!-- StubName can be removed, so no call stub will generated -->\n"
                "    <StubName>_SleepEx@8</StubName>\n"
                "    <!-- For Ordinal and ImportName, you can use either or both -->\n"
                "    <!-- When use both, it's import by name and ordinal works as hint -->\n"
                "    <!-- In most case only ImportName is used, that's enough -->\n"
                "    <!-- But you can't strip both -->\n"
                "    <Ordinal>1208</Ordinal>\n"
                "    <ImportName>SleepEx</ImportName>\n"
                "  </Import>\n"
                "  <Import>\n"
                "    <LinkName>__imp__WaitForSingleObject@8</LinkName>\n"
                "    <ImportName>WaitForSingleObject</ImportName>\n"
                "  </Import>\n"
                "</ImportLibrary>\n"
                "\n"
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?> is needed "
                "for filename or function other than English letters.\n"
            );
        }
    }
    catch (MyMsgException e)
    {
        fprintf(stderr, CT2CA(e.fmt), CT2CA(e.msg));
    }

    CoUninitialize();
}
