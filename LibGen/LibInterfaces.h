#ifndef LIBINTERFACES_H
#define LIBINTERFACES_H

#include "coffInterfaces.h"

namespace Sora
{
    class ILibraryBuilder : public IDispose, public IHasRawData
    {
        public:
            //the name is limited to 14 bytes. No longname is supported.
            virtual void WINAPI AddObject(LPCSTR szName, ICoffBuilder*) = 0;

            //call this method to calculate the offset for first and second link member before retrive raw data
            virtual void WINAPI FillOffsets() = 0;
    };
};

#endif
