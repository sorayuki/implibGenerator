#ifndef IMPFACTORY_H
#define IMPFACTORY_H

#include "ImpInterfaces.h"

namespace Sora {
    extern "C" {
        IImpSectionBuilder* WINAPI GetX86ImpSectionBuilder();
        IImpSectionBuilder* WINAPI GetX64ImpSectionBuilder();
    }
};

#endif
