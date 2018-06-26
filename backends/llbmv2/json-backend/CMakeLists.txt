find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
#message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(LLVMJsonBackend STATIC
    # List your source files here.
    llvm-json.cpp
    emitHeader.cpp
    emitParser.cpp
    emitDeparser.cpp
    helpers.cpp
    emitAction.cpp
    emitControl.cpp
    redundantStoreElimination.cpp
)

# Use C++11 to compile your pass (i.e., supply -std=c++11).
target_compile_features(LLVMJsonBackend PRIVATE cxx_range_for cxx_auto_type)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(LLVMJsonBackend PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)
# Can add those libraries that are not already present in 'p4c-llbm2-ss' binary
llvm_map_components_to_libnames(llvm_libs passes ipo)

target_link_libraries (LLVMJsonBackend ${llvm_libs} )

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(LLVMJsonBackend PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
