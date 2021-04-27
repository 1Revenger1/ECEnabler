#ifndef kern_ece_hpp
#define kern_ece_hpp

#include <Headers/kern_patcher.hpp>

class ECE {
public:
    void init();
    void deinit();
    
    void processKernel(KernelPatcher &patcher);
private:
    static ECE *callbackECE;
    
    static IOReturn ecSpaceHandler(unsigned int, unsigned long, unsigned int, unsigned long *, void*, void *);
    
    mach_vm_address_t orgACPIEC_ecSpaceHandler {};
    
};

#endif //kern_ece_hpp
