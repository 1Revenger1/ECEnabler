#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static const char *kextACPIEC[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/PlugIns/AppleACPIEC.kext/Contents/MacOS/AppleACPIEC" };
static const char *kextACPIPlatform[] { "/System/Library/Extensions/AppleACPIPlatform.kext/Contents/MacOS/AppleACPIPlatform" };

static KernelPatcher::KextInfo kextList[] {
    {"com.apple.driver.AppleACPIEC", kextACPIEC, arrsize(kextACPIEC), {true}, {}, KernelPatcher::KextInfo::Unloaded },
    {"com.apple.driver.AppleACPIPlatform", kextACPIPlatform, arrsize(kextACPIPlatform), {true}, {}, KernelPatcher::KextInfo::Unloaded },
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
    SYSLOG("ECE", "process kext called! Index: %d Addr: %p Size: %x", index, address, size);
    if (index == kextList[0].loadIndex) {
        KernelPatcher::RouteRequest request ("__ZN11AppleACPIEC14ecSpaceHandlerEjmjPmPvS1_", ecSpaceHandler, orgACPIEC_ecSpaceHandler);
        
        if (!patcher.routeMultiple(index, &request, 1, address  , size)) {
            SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", request.symbol, patcher.getError());
            patcher.clearError();
        }
        
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
        }
        
    } else if (index == kextList[1].loadIndex) {
        KernelPatcher::RouteRequest requests[] = {
            {"_AcpiExFieldDatumIo", fieldDatumIO, orgFieldDatumIO},
            {"_AcpiExExtractFromField", extractFromField, orgExtractFromField}
        };
        
        if (!patcher.routeMultiple(index, requests, arrsize(requests), address  , size)) {
            SYSLOG("ECE", "patcher.routeMultiple for %s failed with error %d", requests[0].symbol, patcher.getError());
            patcher.clearError();
        }
    }
}

IOReturn ECE::fieldDatumIO(union acpi_operand_object *obj_desc, UInt32 offset, unsigned long *values,int is_write)
{
    DBGLOG("ECE", "FieldDatumIO Called! Access-Byte-Width: %x Bit-Length: %x Offset: %x", obj_desc->field.access_byte_width, obj_desc->field.bit_length, offset);
    return FunctionCast(fieldDatumIO, callbackECE->orgFieldDatumIO) (obj_desc, offset, values, is_write);
}

IOReturn ECE::extractFromField(union acpi_operand_object *obj_desc, void *buffer, UInt32 buffer_length)
{
    
    IOReturn val = FunctionCast(extractFromField, callbackECE->orgExtractFromField) (
                                                                             obj_desc,
                                                                             buffer,
                                                                             buffer_length
                                                                             );
    
    DBGLOG("ECE", "Extract From Field - Buffer Length: 0x%x Type: %x StartFieldBit: %x", buffer_length, obj_desc->common.type, obj_desc->field.start_field_bit_offset);
    if (buffer_length > 0x8) {
        DBGLOG("ECE", "!!!!!!!!!! BYTE ALLOCATION SIZE: %lu !!!!!!!!!!!!!!!!!!", *((long *)(buffer) - 1));
    }
    return val;
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
        DBGLOG("ECE", "write at %x (%x bits long) with val: %x", addr, bits, *values64);
    } else {
        int bytes = bits / 8;
        
        if (bytes > 8) {
//            bytes = 8;
            DBGLOG("ECE", "Buffer Size = %lu", *((long *) (values64) - 1));
        }
        
        // Do not error out, as this breaks battery methods and prevents status from being read.
        // Set a sane value though
//        if (bytes > 8) {
//            *values64 = 0;
//            return kIOReturnSuccess;
//        }
        
        // Read up to a long (8 bytes), 1 byte at a time
        int maxOffset = bytes;
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
