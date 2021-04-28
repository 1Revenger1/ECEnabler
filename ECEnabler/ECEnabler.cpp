#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC.kext/Contents/MacOS/AppleACPIEC" };

static KernelPatcher::KextInfo kextList[] {
    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

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
    if (index != kextList[0].loadIndex) return;
    SYSLOG("ECE", "process kext called! Index: %d Addr: %p Size: %x", index, address, size);
    
    KernelPatcher::RouteRequest request ("__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_", ecSpaceHandler, orgACPIEC_ecSpaceHandler);
    
    if (!patcher.routeMultiple(index, &request, 1, address  , size)) {
        SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
        patcher.clearError();
    }
}

/**
 * Redirected from AppleACPIEC::ecSpaceHandler (called from AppleACPIPlatform as an address space handler)
 * Will call the original ecSpaceHandler for writes, as well as multiple times for reading
 *
 * TODO: Possibly get IOCommandGate from AppleACPIEC to run our own actions
 */
IOReturn ECE::ecSpaceHandler(unsigned int write, unsigned long addr, unsigned int bits, unsigned char *values64, void *handlerContext, void *RegionContext)
{
    IOReturn result = 0;
    if (addr >= 0x100 || values64 == nullptr || handlerContext == nullptr) {
        return AE_BAD_PARAMETER;
    }
    
    if (write == 1) {
        // Do not modify write requests (for now)
        result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
            write,
            addr,
            bits,
            values64,
            handlerContext,
            RegionContext
        );
        DBGLOG("ECE", "write at %x (%x bits long) with val: %x", addr, bits, *values64);
    } else {
        int bytes = bits / 8;
        
        // Do not error out, as this breaks battery methods and prevents status from being read.
        // Set a sane value though
        if (bytes > 8) {
            *values64 = 0;
            return kIOReturnSuccess;
        }
        
        // Read up to a long (8 bytes), 1 byte at a time
        int maxOffset = min(8, bytes);
        int index = 0;
        do {
            result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
                write,
                addr + index,
                8,
                values64 + index,
                handlerContext,
                RegionContext
            );
            index++;
        } while (index < maxOffset && result == 0);
        DBGLOG("ECE", "read at %x (%x bits long)", addr, bits);
    }
    
    return result;
}
