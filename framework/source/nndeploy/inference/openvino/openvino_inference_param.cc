
#include "nndeploy/inference/openvino/openvino_inference_param.h"

#include "nndeploy/device/device.h"

namespace nndeploy {
namespace inference {

static TypeInferenceParamRegister<
    TypeInferenceParamCreator<OpenVinoInferenceParam>>
    g_openvino_inference_param_register(base::kInferenceTypeOpenVino);

OpenVinoInferenceParam::OpenVinoInferenceParam() : InferenceParam() {
  model_type_ = base::kModelTypeOnnx;
  device_type_ = device::getDefaultHostDeviceType();
  num_thread_ = 4;
}

OpenVinoInferenceParam::OpenVinoInferenceParam(base::InferenceType type)
    : InferenceParam(type) {
  model_type_ = base::kModelTypeOnnx;
  device_type_ = device::getDefaultHostDeviceType();
  num_thread_ = 4;
}

OpenVinoInferenceParam::~OpenVinoInferenceParam() {}

base::Status OpenVinoInferenceParam::set(const std::string &key,
                                         base::Any &any) {
  return base::kStatusCodeOk;
}

base::Status OpenVinoInferenceParam::get(const std::string &key,
                                         base::Any &any) {
  base::Status status = base::kStatusCodeOk;
  return base::kStatusCodeOk;
}

}  // namespace inference
}  // namespace nndeploy
