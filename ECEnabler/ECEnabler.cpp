#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

//static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC/Contents/MacOS/AppleACPIEC" };

//static KernelPatcher::KextInfo kextList[] {
//    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
//};

ECE *ECE::callbackECE = nullptr;

void ECE::init() {
    callbackECE = this;
    
    lilu.onPatcherLoadForce(
    [](void *user, KernelPatcher &patcher) {
        callbackECE->processKernel(patcher);
    }, this);
}

void ECE::processKernel(KernelPatcher &patcher)
{
    DBGLOG("ECE", "process kernel called!");
    
    KernelPatcher::RouteRequest request {"ecSpaceHandler", ecSpaceHandler, orgACPIEC_ecSpaceHandler};
    if (!patcher.routeMultiple(KernelPatcher::KernelID, &request, 1)) {
        SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
        patcher.clearError();
    }
}

IOReturn ECE::ecSpaceHandler(unsigned int param_1, unsigned long addr, unsigned int bits, unsigned long *values64, void *handlerContext, void *RegionContext)
{
    IOReturn result = FunctionCast(ecSpaceHandler, callbackECE->orgACPIEC_ecSpaceHandler) (
        param_1,
        addr,
        bits,
        values64,
        handlerContext,
        RegionContext
    );
    
    SYSLOG("ECE", "ecSpaceHandler is called: %ud %ul %ud %ul %p %p", param_1, addr, bits, values64, handlerContext, RegionContext);
    SYSLOG("ECE", "Return value of %d", result);
    
    return result;
}
