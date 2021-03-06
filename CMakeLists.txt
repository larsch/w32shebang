cmake_minimum_required(VERSION 3.3)

project(w32shebang)
add_compile_options(/W4 /WX /GS- /Oi)
string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS "/nodefaultlib /entry:Main")
add_definitions(-DUNICODE -D_UNICODE)
add_executable(w32shebang w32shebang.c)
target_link_libraries(w32shebang shlwapi)

install(TARGETS w32shebang DESTINATION ".")
install(FILES LICENSE.md DESTINATION "." RENAME LICENSE.txt)
install(FILES README.md DESTINATION "." RENAME README.txt)

execute_process(COMMAND git describe
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "v" "" VERSION ${VERSION})
string(REPLACE "." ";" VERSION ${VERSION})
list(GET VERSION 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET VERSION 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET VERSION 2 CPACK_PACKAGE_VERSION_PATCH)
set(CPACK_GENERATOR "ZIP")
include(CPack)
