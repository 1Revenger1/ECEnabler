//
//  ECEStart.cpp
//  ECEnabler
//
//  Created by Avery Black on 4/26/21.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "ECEnabler.hpp"

static ECE ece;

const char *bootargOff[] {
    "-eceoff"
};

const char *bootargDebug[] {
    "-ecedbg"
};

const char *bootargBeta[] {
    "-ecebeta"
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::Catalina,
    KernelVersion::BigSur,
    []() {
        ece.init();
    }
};
