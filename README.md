# ECEnabler

## This is an experimental Lilu Plugin, not meant for day to day use. I am not responsible for any sort of damage caused by this kext.

Allows reading values at and below the size of 8 bytes, possibly allowing a reduction in the amount of battery patching needed. Anything over 8 bytes will still be read, NOT erroring out, though the value will be truncated to 8 bytes.

Only supports Catalina through Big Sur

## Configuration

`-eceoff` - Disable ECEnabler  
`-ecedbg` - Enable ECEnabler debug logs  
`-ecebeta` - Remove upper OS limit for beta OS  
