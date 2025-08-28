# neoc

neocities cli in c

uses [libcurl](https://curl.se/libcurl/) and no other dependencies

written and tested on macosx. should build fine on unix systems

## compiling

1. download + unzip code
2. `cc api.c cli.c json.c -lcurl`