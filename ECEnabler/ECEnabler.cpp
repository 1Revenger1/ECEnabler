#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static ECE *callbackECE = nullptr;

void ECE::init() {
    callbackECE = this;
    
    lilu.onKextLoadForce(kextList, arrsize(kextList),
    [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
        callbackECE->processKext(patcher, index, address, size);
    }, this);
}

void ECE::deinit()
{
    
}

void ECE::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size)
{
    if (index == kextList[0].loadIndex) {
        const char *ecSpaceHandlerSymbol = newEcSpaceHandlerSymbol;
        if (getKernelVersion() <= KernelVersion::Sierra) {
            ecSpaceHandlerSymbol = oldEcSpaceHandlerSymbol;
        }
        
        KernelPatcher::RouteRequest request (ecSpaceHandlerSymbol, ecSpaceHandler, orgACPIEC_ecSpaceHandler);
        if (!patcher.routeMultiple(index, &request, 1, address  , size)) {
            SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
            patcher.clearError();
        }
    }
}

/**
 * Redirected from AppleACPIEC::ecSpaceHandler (called from AppleACPIPlatform as an address space handler)
 * Will call the original ecSpaceHandler for writes, as well as multiple times for reading
 *
 * @param write         Read = 0 or Write = 1
 * @param addr           Address within EC field
 * @param bits           Size of field, always guaranteed to always be a multiple of 8
 * @param values64  Buffer to read from or write too
 *
 * TODO: Possibly get IOCommandGate from AppleACPIEC to run our own actions
 */
IOReturn ECE::ecSpaceHandler(UInt32 write, UInt64 addr, UInt32 bits, UInt8 *values64, void *handlerContext, void *RegionContext)
{
    int bytes = (bits / 8);
    int maxAddr = 0x100 - bytes;
    IOReturn result = 0;
    // ecSpaceHandler always writes a 64 bit val even if only reading/writing 8 bits from the EC
    UInt64 buffer = 0;
    
    if (addr > maxAddr || values64 == nullptr || handlerContext == nullptr) {
        DBGLOG("ECE", "Addr: 0x%x > MaxAddr: 0x%x", addr, maxAddr);
        return AE_BAD_PARAMETER;
    }
    
    DBGLOG("ECE", "%s @ 0x%x of size: 0x%x", write ? "write" : "read", addr, bits);
    // Split read/writes into 8 bit chunks
    if (write == 1) {
        for (int i = 0; i < bytes && result == 0; i++) {
            DBGLOG("ECE", "0x%lx 0x%x", *(values64 + i), *(values64 + i) & 0xFF);
            buffer = *(values64 + i) & 0xFF;
            result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
                write,
                addr + i,
                8,
                (UInt8 *) &buffer,
                handlerContext,
                RegionContext
            );
        }
    } else {
        for (int i = 0; i < bytes && result == 0; i++) {
            result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
                write,
                addr + i,
                8,
                (UInt8 *) &buffer,
                handlerContext,
                RegionContext
            );
            *(values64 + i) = (UInt8) buffer;
        };
    }
    
    return result;
}
