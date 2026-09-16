set(CMAKE_SYSTEM_NAME WASI)
# LibreSSL uses its i386 portable C implementation for 32-bit little-endian
# targets. Assembly remains disabled by build.sh.
set(CMAKE_SYSTEM_PROCESSOR i386)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_tools_dir "${CMAKE_CURRENT_LIST_DIR}/../tools")
set(CMAKE_C_COMPILER "${_tools_dir}/zig-cc-wasm32-wasi")
set(CMAKE_ASM_COMPILER "${_tools_dir}/zig-cc-wasm32-wasi")
set(CMAKE_AR "${_tools_dir}/zig-ar")
set(CMAKE_RANLIB "${_tools_dir}/zig-ranlib")

set(CMAKE_C_FLAGS_INIT "-Wno-unused-command-line-argument -Dgetuid=getpagesize -Dgeteuid=getpagesize -Dgetgid=getpagesize -Dgetegid=getpagesize")
set(CMAKE_ASM_FLAGS_INIT "-Wno-unused-command-line-argument")
