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
        // Find ecSpaceHandler so that we can narrow down where the mov patch happens
        mach_vm_address_t ecSpaceAddr = patcher.solveSymbol(kextList[0].loadIndex, ecSpaceHandlerSymbol, address, size);
        
        if (patcher.getError() != KernelPatcher::Error::NoError) {
            SYSLOG("ECE", "Failed to find ecSpaceHandler");
            patcher.clearError();
            return;
        }
        
        KernelPatcher::LookupPatch movPatch = {
            &kextList[0],
            nullptr,
            nullptr,
            0,
            1
        };
        
        if (getKernelVersion() == KernelVersion::Lion ||
            getKernelVersion() == KernelVersion::MountainLion) {
            movPatch.find = lionMovPatchFind;
            movPatch.replace = lionMovPatchReplace;
            movPatch.size = sizeof(lionMovPatchFind);
        } else if (getKernelVersion() == KernelVersion::Mavericks) {
            movPatch.find = mavericksMovPatchFind;
            movPatch.replace = mavericksMovPatchReplace;
            movPatch.size = sizeof(mavericksMovPatchFind);
        } else {
            movPatch.find = movPatchFind;
            movPatch.replace = movPatchReplace;
            movPatch.size = sizeof(movPatchFind);
        }
        
        // Do patch within ecSpaceHandler
        size_t maxPatchSize = size - (ecSpaceAddr - address);
        patcher.applyLookupPatch(&movPatch, (UInt8 *) ecSpaceAddr, maxPatchSize);
        
        if (patcher.getError() != KernelPatcher::Error::NoError) {
            SYSLOG("ECE", "Failed to apply ecSpaceHandler MOV patch");
            patcher.clearError();
            // If patch cannot be applied, do not allow function to be routed
            // This can lead to memory and stack corruption!
            kextList[0].switchOff();
            return;
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
