
add_executable(megacli
    megacli.h
    megacli.cpp
)

# Link with SDKlib.
# megacli does not use the intermediate layer.
target_link_libraries(megacli PRIVATE MEGA::SDKlib)

# Adjust compilation flags for warnings and errors
target_platform_compile_options(
    TARGET megacli
    WINDOWS /we4800 # Implicit conversion from 'type' to bool. Possible information loss
    UNIX $<$<CONFIG:Debug>:-ggdb3> -Wall -Wextra -Wconversion
)

if(ENABLE_SDKLIB_WERROR)
    target_platform_compile_options(
        TARGET megacli
        WINDOWS /WX
        UNIX  $<$<CONFIG:Debug>: -Werror>
    )
endif()

# Starting on Xcode 15: https://gitlab.kitware.com/cmake/cmake/-/issues/25297
if(APPLE AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15.0")
    target_link_options(megacli PRIVATE LINKER:-no_warn_duplicate_libraries)
endif()

if(TARGET gfxworker)
    add_custom_command(
        TARGET megacli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:gfxworker> $<TARGET_FILE_DIR:megacli>
        COMMENT "Copying gfxworker for megacli."
    )
endif()
