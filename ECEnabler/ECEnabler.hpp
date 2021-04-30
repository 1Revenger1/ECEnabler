#ifndef kern_ece_hpp
#define kern_ece_hpp

#include <Headers/kern_patcher.hpp>

#define AE_BAD_PARAMETER 0x1001

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC.kext/Contents/MacOS/AppleACPIEC" };
static KernelPatcher::KextInfo kextList[] {
    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

static const char *ecSpaceHandlerSymbol { "__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_" };

// 10.7 - 10.8
static UInt8 lionMovPatchFind[] = {
    0x48, 0x89, 0x0b,   // MOV (%RBX), %RCX
};

static UInt8 lionMovPatchReplace[] = {
    0x40, 0x89, 0x0E,   // MOV (%RBX), %CL
};

// 10.9
static UInt8 mavericksMovPatchFind[] = {
    0x49, 0x89, 0x0E,   // MOV (%R14), %RCX
};

static UInt8 mavericksMovPatchReplace[] = {
    0x41, 0x88, 0x0E,   // MOV (%R14), %CL
};

// 10.10 - 11
static UInt8 movPatchFind[] = {
    0x49, 0x89, 0x06,   // MOV (%R14), %RAX
};
                               
static UInt8 movPatchReplace[] = {
    0x41, 0x88, 0x06,   // MOV (%R14), %AL
};

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
