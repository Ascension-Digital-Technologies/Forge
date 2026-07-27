if(NOT DEFINED BUILD_DIR OR NOT DEFINED SOURCE_DIR OR NOT DEFINED CMAKE_COMMAND_PATH)
  message(FATAL_ERROR "BUILD_DIR, SOURCE_DIR, and CMAKE_COMMAND_PATH are required")
endif()

set(prefix "${BUILD_DIR}/install-consumer-prefix")
set(consumer_build "${BUILD_DIR}/install-consumer-build")
set(consumer_sanitizer_args "")
if(BUILD_DIR MATCHES "asan-ubsan")
  list(APPEND consumer_sanitizer_args
    "-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
    "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined")
endif()
file(REMOVE_RECURSE "${prefix}" "${consumer_build}")

execute_process(
  COMMAND "${CMAKE_COMMAND_PATH}" --install "${BUILD_DIR}" --prefix "${prefix}"
  RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Forge install step failed: ${install_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND_PATH}" -S "${SOURCE_DIR}/tests/package/consumer" -B "${consumer_build}"
          -DCMAKE_PREFIX_PATH=${prefix} ${consumer_sanitizer_args}
  RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
  message(FATAL_ERROR "Forge consumer configure failed: ${configure_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND_PATH}" --build "${consumer_build}"
  RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "Forge consumer build failed: ${build_result}")
endif()

if(WIN32)
  set(executable "${consumer_build}/Debug/forge-consumer.exe")
  if(NOT EXISTS "${executable}")
    set(executable "${consumer_build}/forge-consumer.exe")
  endif()
else()
  set(executable "${consumer_build}/forge-consumer")
endif()

execute_process(COMMAND "${executable}" RESULT_VARIABLE run_result OUTPUT_VARIABLE output)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "Forge consumer failed: ${run_result}")
endif()
string(STRIP "${output}" output)
if(NOT output STREQUAL "1.0.0")
  message(FATAL_ERROR "Unexpected installed Forge version: ${output}")
endif()


set(c_consumer_build "${BUILD_DIR}/install-c-consumer-build")
file(REMOVE_RECURSE "${c_consumer_build}")
execute_process(
  COMMAND "${CMAKE_COMMAND_PATH}" -S "${SOURCE_DIR}/tests/package/c-consumer" -B "${c_consumer_build}"
          -DCMAKE_PREFIX_PATH=${prefix} ${consumer_sanitizer_args}
  RESULT_VARIABLE c_configure_result)
if(NOT c_configure_result EQUAL 0)
  message(FATAL_ERROR "Forge C consumer configure failed: ${c_configure_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND_PATH}" --build "${c_consumer_build}" RESULT_VARIABLE c_build_result)
if(NOT c_build_result EQUAL 0)
  message(FATAL_ERROR "Forge C consumer build failed: ${c_build_result}")
endif()
if(WIN32)
  set(c_executable "${c_consumer_build}/Debug/forge-c-consumer.exe")
  if(NOT EXISTS "${c_executable}")
    set(c_executable "${c_consumer_build}/forge-c-consumer.exe")
  endif()
else()
  set(c_executable "${c_consumer_build}/forge-c-consumer")
endif()
execute_process(COMMAND "${c_executable}" RESULT_VARIABLE c_run_result OUTPUT_VARIABLE c_output)
if(NOT c_run_result EQUAL 0 OR NOT c_output MATCHES "module @installed_c_api")
  message(FATAL_ERROR "Forge installed C API consumer failed: ${c_run_result} ${c_output}")
endif()
