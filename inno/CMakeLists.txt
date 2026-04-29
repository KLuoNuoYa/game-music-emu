set(gme_inno_SRCS
    gme_inno.cpp
    gme_inno.h
    "${CMAKE_CURRENT_BINARY_DIR}/gme_inno.def"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(GMEINNO_SYM_OPEN_FILE "_GMEInnoOpenFileW@8")
    set(GMEINNO_SYM_OPEN_FILE_TRACK "_GMEInnoOpenFileTrackW@12")
    set(GMEINNO_SYM_CLOSE "_GMEInnoClose@0")
    set(GMEINNO_SYM_START_TRACK "_GMEInnoStartTrack@4")
    set(GMEINNO_SYM_PLAY "_GMEInnoPlay@0")
    set(GMEINNO_SYM_PAUSE "_GMEInnoPause@0")
    set(GMEINNO_SYM_STOP "_GMEInnoStop@0")
    set(GMEINNO_SYM_IS_PLAYING "_GMEInnoIsPlaying@0")
    set(GMEINNO_SYM_SET_LOOP "_GMEInnoSetLoop@4")
    set(GMEINNO_SYM_SET_VOLUME "_GMEInnoSetVolume@4")
    set(GMEINNO_SYM_GET_TRACK_COUNT "_GMEInnoGetTrackCount@0")
    set(GMEINNO_SYM_GET_CURRENT_TRACK "_GMEInnoGetCurrentTrack@0")
    set(GMEINNO_SYM_GET_LAST_ERROR "_GMEInnoGetLastErrorW@8")
    set(GMEINNO_SYM_GET_LAST_ERROR_LENGTH "_GMEInnoGetLastErrorLength@0")
else()
    set(GMEINNO_SYM_OPEN_FILE "GMEInnoOpenFileW")
    set(GMEINNO_SYM_OPEN_FILE_TRACK "GMEInnoOpenFileTrackW")
    set(GMEINNO_SYM_CLOSE "GMEInnoClose")
    set(GMEINNO_SYM_START_TRACK "GMEInnoStartTrack")
    set(GMEINNO_SYM_PLAY "GMEInnoPlay")
    set(GMEINNO_SYM_PAUSE "GMEInnoPause")
    set(GMEINNO_SYM_STOP "GMEInnoStop")
    set(GMEINNO_SYM_IS_PLAYING "GMEInnoIsPlaying")
    set(GMEINNO_SYM_SET_LOOP "GMEInnoSetLoop")
    set(GMEINNO_SYM_SET_VOLUME "GMEInnoSetVolume")
    set(GMEINNO_SYM_GET_TRACK_COUNT "GMEInnoGetTrackCount")
    set(GMEINNO_SYM_GET_CURRENT_TRACK "GMEInnoGetCurrentTrack")
    set(GMEINNO_SYM_GET_LAST_ERROR "GMEInnoGetLastErrorW")
    set(GMEINNO_SYM_GET_LAST_ERROR_LENGTH "GMEInnoGetLastErrorLength")
endif()

configure_file(gme_inno.def.in gme_inno.def @ONLY)

add_library(gme_inno SHARED ${gme_inno_SRCS})
set_target_properties(gme_inno PROPERTIES OUTPUT_NAME gme_inno)

target_compile_definitions(gme_inno PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(gme_inno PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(gme_inno PRIVATE gme_static winmm)
target_compile_definitions(gme_inno PRIVATE GME_INNO_BUILD_DLL)

install(TARGETS gme_inno
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES gme_inno.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gme)
