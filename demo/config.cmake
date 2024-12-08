message(STATUS "Building nndeploy demo")

# framework
if(ENABLE_NNDEPLOY_BASE)
  include(${ROOT_PATH}/demo/base/config.cmake)
endif()

if(ENABLE_NNDEPLOY_THREAD_POOL)
  include(${ROOT_PATH}/demo/thread_pool/config.cmake)
endif()

if(ENABLE_NNDEPLOY_DEVICE)
  include(${ROOT_PATH}/demo/device/config.cmake)

  if(ENABLE_NNDEPLOY_DEVICE_ASCEND_CL)
    include(${ROOT_PATH}/demo/ascend_cl/config.cmake)
  endif()
endif()

if(ENABLE_NNDEPLOY_IR)
  include(${ROOT_PATH}/demo/ir/config.cmake)
endif()

if(ENABLE_NNDEPLOY_OP)
  include(${ROOT_PATH}/demo/op/config.cmake)
endif()

if(ENABLE_NNDEPLOY_NET)
  include(${ROOT_PATH}/demo/net/config.cmake)
endif()

if(ENABLE_NNDEPLOY_INFERENCE)
  include(${ROOT_PATH}/demo/inference/config.cmake)
endif()

if(ENABLE_NNDEPLOY_DAG)
  include(${ROOT_PATH}/demo/dag/config.cmake)
endif()

# plugin
if(ENABLE_NNDEPLOY_PLUGIN_PREPROCESS)
  include(${ROOT_PATH}/demo/preprocess/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_INFER)
  include(${ROOT_PATH}/demo/infer/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_CODEC)
  include(${ROOT_PATH}/demo/codec/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_CLASSIFICATION)
  include(${ROOT_PATH}/demo/classification/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_DETECT)
  include(${ROOT_PATH}/demo/detect/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_SEGMENT)
  include(${ROOT_PATH}/demo/segment/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_TOKENIZER_CPP)
  include(${ROOT_PATH}/demo/tokenizer_cpp/config.cmake)
endif()

if(ENABLE_NNDEPLOY_PLUGIN_STABLE_DIFFUSION)
  include(${ROOT_PATH}/demo/stable_diffusion/config.cmake)
endif()

nndeploy_option(ENABLE_NNDEPLOY_DEMO_LLAMA "ENABLE_NNDEPLOY_DEMO_LLAMA" OFF)
if(ENABLE_NNDEPLOY_DEMO_LLAMA)
  include(${ROOT_PATH}/demo/llama/config.cmake)
endif()

nndeploy_option(ENABLE_NNDEPLOY_DEMO_TENSOR_POOL "ENABLE_NNDEPLOY_DEMO_TENSOR_POOL" OFF)
if(ENABLE_NNDEPLOY_DEMO_TENSOR_POOL)
  include(${ROOT_PATH}/demo/tensor_pool/config.cmake)
endif()

nndeploy_option(ENABLE_NNDEPLOY_DEMO_RESNET "ENABLE_NNDEPLOY_DEMO_RESNET" OFF)
if(ENABLE_NNDEPLOY_DEMO_RESNET)
  include(${ROOT_PATH}/demo/resnet/config.cmake)
endif()

if(ENABLE_NNDEPLOY_OP_ASCEND_C)
  include(${ROOT_PATH}/demo/acl_op/config.cmake)
endif()