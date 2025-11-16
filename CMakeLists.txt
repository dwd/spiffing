cmake_minimum_required(VERSION 3.10)
project(spiffing)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

find_package(asn1c)
find_package(flxml)

set(ASN1_SOURCES ESSSecurityLabel.asn Clearance.asn SSLPrivileges.asn MissiSecurityCategories.asn acp145.asn)

# Handle build type - uppercase it for variable name matching
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)

# Set ASN1C paths based on build type
set(ASN1C ${asn1c_BIN_DIRS_${BUILD_TYPE_UPPER}}/asn1c)
set(ASN1C_SKELETON ${asn1c_RES_DIRS_${BUILD_TYPE_UPPER}})

# Debug: Print what we found
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_TYPE_UPPER: ${BUILD_TYPE_UPPER}")
message(STATUS "asn1c_BIN_DIRS_${BUILD_TYPE_UPPER}: ${asn1c_BIN_DIRS_${BUILD_TYPE_UPPER}}")
message(STATUS "ASN1C: ${ASN1C}")
message(STATUS "ASN1C_SKELETON: ${ASN1C_SKELETON}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen-ber)
set(ASN1C_FILES)
foreach(ASN1_FILE ${ASN1_SOURCES})
    set(ASN1C_FILES ${ASN1C_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/${ASN1_FILE})
endforeach(ASN1_FILE)

# Generate files at configure time to know what files exist
if(asn1c_FOUND)
    message(STATUS "Running: ${ASN1C} -S ${ASN1C_SKELETON} -fwide-types -fcompound-names ${ASN1C_FILES}")
    execute_process(
        COMMAND ${ASN1C} -S "${ASN1C_SKELETON}" -fwide-types -fcompound-names ${ASN1C_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen-ber
        RESULT_VARIABLE ASN1C_RESULT
        COMMAND_ECHO STDOUT
    )
    message(STATUS "asn1c completed with result: ${ASN1C_RESULT}")
    message("Found and executed asn1c as ${ASN1C} -S ${ASN1C_SKELETON}")
endif()

message("Globbing output files")
# Now glob the generated files
file(GLOB ASN_C_FILES ${CMAKE_CURRENT_BINARY_DIR}/gen-ber/*.c ${ASN1C_SKELETON}/asn1c/*.c)
# Remove converter-sample.c from the list
list(FILTER ASN_C_FILES EXCLUDE REGEX "converter-sample\\.c$")
message("Found ${ASN_C_FILES}")

set(ASN_SOURCE_FILES ${ASN_C_FILES})
set_property(SOURCE ${ASN_SOURCE_FILES} PROPERTY GENERATED TRUE)

# Re-generate at build time in case ASN1 sources change
add_custom_target(gen-ber
        DEPENDS ${ASN1_SOURCES} ${ASN1C}
        COMMAND ${ASN1C} -S "${ASN1C_SKELETON}" -fwide-types -fcompound-names ${ASN1C_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gen-ber)
add_dependencies(gen-ber asn1c::asn1c)

add_library(spiffing-asn ${ASN_SOURCE_FILES})
add_dependencies(spiffing-asn gen-ber)
target_link_libraries(spiffing-asn asn1c::asn1c)
target_include_directories(spiffing-asn PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/gen-ber ${ASN1C_SKELETON}/asn1c)
target_link_libraries(spiffing-asn asn1c::asn1c)

set(SOURCE_FILES
    src/category.cc
    src/categorydata.cc
    src/categorygroup.cc
    src/catutils.cc
    src/classification.cc
    src/clearance.cc
    src/label.cc
    src/lacv.cc
    src/marking.cc
    src/spif.cc
    src/tag.cc
    src/tagset.cc include/spiffing/spiffing.h src/spiffing.cc src/equivclass.cc include/spiffing/equivclass.h src/equivcat.cc include/spiffing/equivcat.h src/constants.cc include/spiffing//constants.h include/spiffing/markings.h src/markings.cc include/spiffing/exceptions.h src/exceptions.cc)
include_directories(include)
add_library(spiffing ${SOURCE_FILES})
add_dependencies(spiffing spiffing-asn)
target_link_libraries(spiffing spiffing-asn flxml::flxml)

add_executable(spifflicator spifflicator.cc)
add_dependencies(spifflicator spiffing)
target_link_libraries(spifflicator spiffing)

add_executable(test-spiffing test.cc)
add_dependencies(test-spiffing spiffing)
target_link_libraries(test-spiffing spiffing spiffing-asn)

add_executable(transpifferizer transpifferizer.cc)
target_link_libraries(transpifferizer spiffing spiffing-asn)
