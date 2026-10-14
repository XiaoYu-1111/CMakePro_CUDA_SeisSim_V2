# Ninja compares /showIncludes bytes with this prefix. Keep compiler output UTF-8
# and probe the installed language; VSLANG cannot select an uninstalled language.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    if(NOT SEISSIM_MSVC_DEPENDENCIES_CONFIGURED)
        set(seisSimMsvcLauncher cmd.exe /d /c call "${CMAKE_CURRENT_LIST_DIR}/MsvcCompile.cmd")
        set(seisSimProbeDir "${CMAKE_BINARY_DIR}/CMakeFiles/SeisSimShowIncludes")
        file(MAKE_DIRECTORY "${seisSimProbeDir}")
        file(WRITE "${seisSimProbeDir}/probe.h" "/* Header dependency probe. */\n")
        file(WRITE "${seisSimProbeDir}/probe.c" "#include \"probe.h\"\nint main(void) { return 0; }\n")
        execute_process(
            COMMAND ${seisSimMsvcLauncher} "${CMAKE_CXX_COMPILER}"
                    /nologo /showIncludes /c probe.c /Foprobe.obj
            WORKING_DIRECTORY "${seisSimProbeDir}"
            OUTPUT_VARIABLE seisSimIncludeOutput ERROR_VARIABLE seisSimIncludeError
            RESULT_VARIABLE seisSimProbeResult ENCODING UTF-8
        )
        # The probe contains one include, so everything before its absolute path is the prefix.
        if(NOT seisSimProbeResult EQUAL 0 OR NOT seisSimIncludeOutput MATCHES "(^|\n)([^\r\n]+: +)[A-Za-z]:[/\\\\][^\r\n]*probe\\.h")
            message(FATAL_ERROR "Cannot detect MSVC header dependencies: ${seisSimIncludeOutput}${seisSimIncludeError}")
        endif()
        set(SEISSIM_MSVC_INCLUDE_PREFIX "${CMAKE_MATCH_2}")
        foreach(seisSimLanguage IN ITEMS C CXX)
            list(PREPEND CMAKE_${seisSimLanguage}_COMPILER_LAUNCHER ${seisSimMsvcLauncher})
        endforeach()
        set(SEISSIM_MSVC_DEPENDENCIES_CONFIGURED TRUE)
    endif()
    # Also override old compiler-info files when opening an existing VS build cache.
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${SEISSIM_MSVC_INCLUDE_PREFIX}")
    set(CMAKE_C_CL_SHOWINCLUDES_PREFIX "${SEISSIM_MSVC_INCLUDE_PREFIX}")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${SEISSIM_MSVC_INCLUDE_PREFIX}")
endif()
