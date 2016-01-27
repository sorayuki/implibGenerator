#ifndef COFFFACTORIES_H
#define COFFFACTORIES_H

#include "coffInterfaces.h"

namespace Sora {
    extern "C" {
        ICoffFactory* WINAPI GetX86CoffFactory();
        ICoffFactory* WINAPI GetX64CoffFactory();
        ICoffFactory* WINAPI GetIA64CoffFactory();
    }
};

#endif
