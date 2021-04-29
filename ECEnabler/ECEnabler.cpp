#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC.kext/Contents/MacOS/AppleACPIEC" };
static KernelPatcher::KextInfo kextList[] {
    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

static UInt8 movPatchFind[] = {0x49, 0x89, 0x06,    // MOV (%R14), RAX
                               0xeb, 0x34};         // JMP 0x36
static UInt8 movPatchReplace[] = {0x41, 0x88, 0x06, // MOV (%R14), %AL
                                  0xeb, 0x34};      // JMP 0x36

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
        KernelPatcher::RouteRequest request ("__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_", ecSpaceHandler, orgACPIEC_ecSpaceHandler);
        
        const KernelPatcher::LookupPatch movPatch = {
            &kextList[0],
            movPatchFind, // MOV (%R14),RAX
            movPatchReplace, // MOV (%R14),AL
            sizeof(movPatchFind),
            1
        };
        
        patcher.applyLookupPatch(&movPatch);
        
        if (patcher.getError() != KernelPatcher::Error::NoError) {
            SYSLOG("ECE", "Failed to apply ecSpaceHandler MOV patch");
            patcher.clearError();
            // If patch cannot be applied, do not allow function to be routed
            // This can lead to memory and stack corruption!
            kextList[0].switchOff();
            return;
        }
        
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
 * TODO: Possibly get IOCommandGate from AppleACPIEC to run our own actions
 */
IOReturn ECE::ecSpaceHandler(UInt32 write, UInt64 addr, UInt32 bits, UInt8 *values64, void *handlerContext, void *RegionContext)
{
    int maxAddr = 0x100 - (bits / 8);
    IOReturn result = 0;
    if (addr > maxAddr || values64 == nullptr || handlerContext == nullptr) {
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
    } else {
        // Split read into 1 byte chunks
        int maxOffset = bits / 8;
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
    }
    
    return result;
}
