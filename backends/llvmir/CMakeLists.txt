# IIT Hyderabad

# authors:
# Venkata Keerthy, Pankaj K, Bhanu Prakash T, D Tharun Kumar
# {cs17mtech11018, cs15btech11029, cs15btech11037, cs15mtech11002}@iith.ac.in

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
message(STATUS "This is a cmake file from LLVMIR")

set (P4C_LLVMIR_SRCS
	p4c-llvmir.cpp
  midend.cpp
  )

set (P4C_LLVMIR_HDRS
	LLVMIROptions.h
  midend.h
  )

add_cpplint_files(${CMAKE_CURRENT_SOURCE_DIR} "${P4C_LLVMIR_SRCS};${P4C_LLVMIR_HDRS}")

# set (P4C_EBPF_DIST_HEADERS p4include/ebpf_model.p4)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

build_unified(P4C_LLVMIR_SRCS ALL)
add_executable(p4c-llvmir ${P4C_LLVMIR_SRCS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries (p4c-llvmir ${llvm_libs} ${P4C_LIBRARIES} ${P4C_LIB_DEPS})
add_dependencies(p4c-llvmir genIR)

install (TARGETS p4c-llvmir
  RUNTIME DESTINATION ${P4C_RUNTIME_OUTPUT_DIRECTORY})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/p4include
  DESTINATION ${P4C_ARTIFACTS_OUTPUT_DIRECTORY})
