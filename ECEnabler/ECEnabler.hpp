#ifndef kern_ece_hpp
#define kern_ece_hpp

#include <Headers/kern_patcher.hpp>

#define AE_BAD_PARAMETER 0x1001

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC.kext/Contents/MacOS/AppleACPIEC" };
static KernelPatcher::KextInfo kextList[] {
    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

static const char *oldEcSpaceHandlerSymbol { "__ZN11AppleACPIEC14ecSpaceHandlerEjyjPyPvS1_" }; // 10.7-10.12
static const char *newEcSpaceHandlerSymbol { "__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_" }; // 10.13-???

class ECE {
public:
    void init();
    void deinit();
    
private:
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t addres, size_t size);
    
    static IOReturn ecSpaceHandler(UInt32, UInt64, UInt32, UInt8 *, void*, void *);
    
    mach_vm_address_t orgACPIEC_ecSpaceHandler {0};
};

#endif //kern_ece_hpp
