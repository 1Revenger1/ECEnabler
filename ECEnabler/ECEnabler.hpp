#ifndef kern_ece_hpp
#define kern_ece_hpp

#include <Headers/kern_patcher.hpp>

#define AE_BAD_PARAMETER 0x1001

#pragma pack(8)

#define ACPI_OBJECT_COMMON_HEADER \
    union acpi_operand_object *next_object; \
    UInt8 descriptor_type; \
    UInt8 type; \
    UInt16 reference_count; \
    UInt8 flags; \

struct acpi_object_common {
    ACPI_OBJECT_COMMON_HEADER
};

struct acpi_object_field {
    ACPI_OBJECT_COMMON_HEADER
    UInt8 field_flags;
    UInt8 attribute;
    UInt8 access_byte_width;
    void *node;
    UInt32 bit_length;
    UInt32 base_byte_offset;
    UInt32 value;
    UInt8 start_field_bit_offset;
    UInt8 access_length;
};

static_assert(sizeof(acpi_object_common) == 16, "Invalid acpi_object_common size");
static_assert(sizeof(acpi_object_field) == 40, "Invalid acpi_object_field size");

// From Linux/drivers/acpi/acpica/acobject.h
union acpi_operand_object {
    struct acpi_object_common common;
    struct acpi_object_field field;
};

#pragma pack()

class ECE {
public:
    void init();
    void deinit();
    
private:
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t addres, size_t size);
    
    static IOReturn ecSpaceHandler(UInt32, UInt64, UInt32, UInt8 *, void*, void *);
    static IOReturn extractFromField(union acpi_operand_object *, void *, UInt32);
    static IOReturn fieldDatumIO(union acpi_operand_object *, UInt32, unsigned long * ,int);
    
    mach_vm_address_t orgACPIEC_ecSpaceHandler {0};
    mach_vm_address_t orgExtractFromField {0};
    mach_vm_address_t orgFieldDatumIO {0};
    
};

#endif //kern_ece_hpp
