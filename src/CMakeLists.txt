
set(SOURCE_FILES
        9p2000.cpp
        debug.cpp
        decoder.cpp
        encoder.cpp
        requestWriter.cpp
        responseWriter.cpp
        )

add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${CONAN_LIBS})

install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
