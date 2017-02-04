include ("../CMake.def")

set(CMAKE_INSTALL_PREFIX "../../dist")

set (TARGET_NAME vbr_server)

set (SOURCES 
     main.cpp
    )	
 
set (HEADERS
    )

add_executable (${TARGET_NAME} ${HEADERS} ${SOURCES})
target_link_libraries (${TARGET_NAME} boost_program_options boost_thread boost_system )

install(TARGETS ${TARGET_NAME} DESTINATION "bin" COMPONENT ${TARGET_NAME})
