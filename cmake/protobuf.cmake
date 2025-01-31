include(ExternalProject)
include(cmake/util.cmake)

if (ENABLE_NNDEPLOY_PROTOBUF STREQUAL "OFF")
elseif (ENABLE_NNDEPLOY_PROTOBUF STREQUAL "ON")
  option(protobuf_BUILD_TESTS "" OFF)
  option(protobuf_MSVC_STATIC_RUNTIME "" ${ONNX_USE_MSVC_STATIC_RUNTIME})

  find_package(Protobuf REQUIRED)
  if(Protobuf_FOUND)
    include_directories(${Protobuf_INCLUDE_DIRS})
    set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} ${Protobuf_LIBRARIES})
  else()
    set(LIBS libprotobuf)
    add_subdirectory_if_no_target(${PROJECT_SOURCE_DIR}/third_party/protobuf ${LIBS})
    include_directories(${PROBUF_ROOT}/src)
    set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} ${LIBS})
  endif()
else()
endif()
