#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC/Contents/MacOS/AppleACPIEC" };

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
    mach_vm_address_t funcAddr = patcher.solveSymbol(index, "__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_", address, size, false);
    SYSLOG("ECE", "process kext called! Index: %d Addr: %p Size: %x FuncAddr: %p", index, address, size, funcAddr);
    
    KernelPatcher::RouteRequest request ("__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_", ecSpaceHandler, orgACPIEC_ecSpaceHandler);
    
//    if (!patcher.routeMultiple(index, &request, 1, address  , size)) {
//        SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
//        patcher.clearError();
//    }
}

IOReturn ECE::ecSpaceHandler(unsigned int param_1, unsigned long addr, unsigned int bits, unsigned long *values64, void *handlerContext, void *RegionContext)
{
//    // Call original function
//    IOReturn result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
//        param_1,
//        addr,
//        bits,
//        values64,
//        handlerContext,
//        RegionContext
//    );
    
    SYSLOG("ECE", "ecSpaceHandler is called: %ud %ul %ud %ul %p %p", param_1, addr, bits, values64, handlerContext, RegionContext);
//    SYSLOG("ECE", "Return value of %d", result);
    
    return 0x1001;
}
