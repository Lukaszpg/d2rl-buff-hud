if(NOT DEFINED D2RL_COMPILER OR D2RL_COMPILER STREQUAL "")
    message(FATAL_ERROR "D2RL_COMPILER was not provided.")
endif()
if(NOT EXISTS "${D2RL_COMPILER}")
    message(FATAL_ERROR "D2RLCompiler.exe not found at: ${D2RL_COMPILER}")
endif()
if(NOT DEFINED BUFF_PANEL_COMPANION_DIR OR NOT IS_DIRECTORY "${BUFF_PANEL_COMPANION_DIR}")
    message(FATAL_ERROR "Buff Panel companion directory is missing: ${BUFF_PANEL_COMPANION_DIR}")
endif()
if(NOT DEFINED BUFF_PANEL_COMPANION_OUTPUT OR BUFF_PANEL_COMPANION_OUTPUT STREQUAL "")
    message(FATAL_ERROR "BUFF_PANEL_COMPANION_OUTPUT was not provided.")
endif()

get_filename_component(output_dir "${BUFF_PANEL_COMPANION_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

execute_process(
    COMMAND "${D2RL_COMPILER}" pack "${BUFF_PANEL_COMPANION_DIR}"
        --output "${BUFF_PANEL_COMPANION_OUTPUT}"
    RESULT_VARIABLE pack_result
    OUTPUT_VARIABLE pack_stdout
    ERROR_VARIABLE pack_stderr
)

if(NOT pack_stdout STREQUAL "")
    message(STATUS "${pack_stdout}")
endif()
if(NOT pack_result EQUAL 0)
    message(FATAL_ERROR "D2RLCompiler pack failed with exit code ${pack_result}: ${pack_stderr}")
endif()
if(NOT EXISTS "${BUFF_PANEL_COMPANION_OUTPUT}")
    message(FATAL_ERROR "D2RLCompiler reported success but did not create: ${BUFF_PANEL_COMPANION_OUTPUT}")
endif()
