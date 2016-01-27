#include <stdio.h>
#include "ImpLibFix.h"

int main()
{
    FILE* f = fopen("ims.lib", "rb+");
    fseek(f, 0, SEEK_END);
    int fs = ftell(f);
    fseek(f, 0, SEEK_SET);

    PBYTE buf = new BYTE[fs];
    fread(buf, 1, fs, f);

    Sora::RenameImpLibObjects("member/", buf, fs);

    fseek(f, 0, SEEK_SET);
    fwrite(buf, 1, fs, f);

    fclose(f);

    delete[] buf;
}
