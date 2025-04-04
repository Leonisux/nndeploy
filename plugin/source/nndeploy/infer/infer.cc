
#include "nndeploy/infer/infer.h"

namespace nndeploy {
namespace infer {

Infer::Infer(const std::string &name) : dag::Node(name) {}
Infer::Infer(const std::string &name, std::vector<dag::Edge *> inputs,
             std::vector<dag::Edge *> outputs)
    : dag::Node(name, inputs, outputs) {}

Infer::Infer(const std::string &name, base::InferenceType type)
    : dag::Node(name) {
  type_ = type;
  inference_ = inference::createInference(type);
  if (inference_ == nullptr) {
    NNDEPLOY_LOGE("Failed to create inference");
    constructed_ = false;
  } else {
    constructed_ = true;
  }
}
Infer::Infer(const std::string &name, std::vector<dag::Edge *> inputs,
             std::vector<dag::Edge *> outputs, base::InferenceType type)
    : dag::Node(name, inputs, outputs) {
  type_ = type;
  inference_ = inference::createInference(type);
  if (inference_ == nullptr) {
    NNDEPLOY_LOGE("Failed to create inference");
    constructed_ = false;
  } else {
    constructed_ = true;
  }
}

Infer::~Infer() {}

base::Status Infer::setInferenceType(base::InferenceType inference_type) {
  if (inference_ == nullptr) {
    type_ = inference_type;
    inference_ = inference::createInference(type_);
    if (inference_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create inference");
      return base::kStatusCodeErrorInvalidParam;
    }
    return base::kStatusCodeOk;
  } else {
    NNDEPLOY_LOGW("inference is not nullptr");
    return base::kStatusCodeErrorInvalidParam;
  }
}

base::Status Infer::setParam(base::Param *param) {
  base::Status status = base::kStatusCodeOk;
  status = inference_->setParam(param);
  return status;
}
base::Param *Infer::getParam() { return inference_->getParam(); }

base::Status Infer::setParamSharedPtr(std::shared_ptr<base::Param> param) {
  base::Status status = base::kStatusCodeOk;
  NNDEPLOY_CHECK_PARAM_NULL_RET_STATUS(param, "param is nullptr");
  status = inference_->setParamSharedPtr(param);
  return status;
}
std::shared_ptr<base::Param> Infer::getParamSharedPtr() {
  return inference_->getParamSharedPtr();
}

base::Status Infer::init() {
  base::Status status = base::kStatusCodeOk;

  if (!is_external_stream_ && stream_ == nullptr) {
    stream_ = device::createStream(device_type_);
  }
  if (device_type_ == inference_->getDeviceType() ||
      (device::isHostDeviceType(device_type_) &&
       device::isHostDeviceType(inference_->getDeviceType()))) {
    inference_->setStream(stream_);
  }
  status = inference_->init();
  NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk,
                         "abstract_inference init failed");
  is_input_dynamic_ = inference_->isInputDynamic();
  is_output_dynamic_ = inference_->isOutputDynamic();
  can_op_input_ = inference_->canOpInput();
  can_op_output_ = inference_->canOpOutput();
  return status;
}
base::Status Infer::deinit() {
  base::Status status = base::kStatusCodeOk;
  status = inference_->deinit();
  NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk, "deinit failed");
  return status;
}

int64_t Infer::getMemorySize() { return inference_->getMemorySize(); }
base::Status Infer::setMemory(device::Buffer *buffer) {
  return inference_->setMemory(buffer);
}

// base::Status Infer::run() {
//   NNDEPLOY_LOGE("Infer::run!Thread ID: %d.\n", std::this_thread::get_id());
//   base::Status status = base::kStatusCodeOk;
//   int index = inputs_[0]->getIndex(this);
//   for (int i = 1; i < inputs_.size(); i++) {
//     int compare_index = inputs_[i]->getIndex(this);
//     if (index != compare_index) {
//       NNDEPLOY_LOGE("index not equal");
//       return base::kStatusCodeErrorInvalidValue;
//     }
//   }
//   if (is_input_dynamic_) {
//     base::ShapeMap shape_map;
//     for (auto input : inputs_) {
//       device::Tensor *tensor = input->getTensor(this);
//       shape_map[tensor->getName()] = tensor->getShape();
//     }
//     inference_->reshape(shape_map);
//   }
//   for (auto input : inputs_) {
//     device::Tensor *tensor = input->getTensor(this);
//     inference_->setInputTensor(tensor->getName(), tensor);
//   }
//   status = inference_->run();
//   NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk, "run failed");
//   for (auto output : outputs_) {
//     std::string name = output->getName();
//     base::ParallelType parallel_type = output->getParallelType();
//     bool flag = parallel_type == base::kParallelTypePipeline;
//     device::Tensor *tensor =
//         inference_->getOutputTensorAfterRun(name, device_type_, flag);
//     output->set(tensor, index, false);
//   }
//   NNDEPLOY_LOGE("infer!\n");
//   return status;
// }

base::Status Infer::run() {
  // NNDEPLOY_LOGE("Infer::run!Thread ID: %d.\n", std::this_thread::get_id());
  base::Status status = base::kStatusCodeOk;
  std::vector<device::Tensor *> tensors;
  std::vector<int> indexs;
  for (auto input : inputs_) {
    device::Tensor *tensor = input->getTensor(this);
    tensors.emplace_back(tensor);
    int index = input->getIndex(this);
    indexs.emplace_back(index);
  }
  int index = indexs[0];
  for (int i = 1; i < indexs.size(); i++) {
    if (index != indexs[i]) {
      NNDEPLOY_LOGE("index not equal");
      return base::kStatusCodeErrorInvalidValue;
    }
  }
  if (is_input_dynamic_) {
    base::ShapeMap shape_map;
    for (auto tensor : tensors) {
      shape_map[tensor->getName()] = tensor->getShape();
    }
    inference_->reshape(shape_map);
  }
  for (auto tensor : tensors) {
    inference_->setInputTensor(tensor->getName(), tensor);

#if 0
    std::string name = tensor->getName();
    std::string filename = name + ".csv";
    size_t pos = 0;
    while ((pos = filename.find('/')) != std::string::npos) {
      filename.replace(pos, 1, "_");
    }
    std::ofstream output_file(filename, std::ios::trunc);
    if (output_file.is_open()) {
      tensor->print(output_file);
      output_file.close();
    } else {
      NNDEPLOY_LOGE("无法打开文件：%s", filename.c_str());
    }
#endif
  }
  status = inference_->run();
  NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk, "run failed");
  for (auto output : outputs_) {
    std::string name = output->getName();
    base::ParallelType parallel_type = output->getParallelType();
    bool flag = parallel_type == base::kParallelTypePipeline;
    device::Tensor *tensor =
        inference_->getOutputTensorAfterRun(name, device_type_, flag);
    if (tensor == nullptr) {
      NNDEPLOY_LOGE("can't getOutputTensorAfterRun[%s].\n", name.c_str());
      status = base::kStatusCodeErrorInvalidParam;
      break;
    }

#if 0
    std::string filename = name + ".csv";
    size_t pos = 0;
    while ((pos = filename.find('/')) != std::string::npos) {
      filename.replace(pos, 1, "_");
    }
    std::ofstream output_file(filename, std::ios::trunc);
    if (output_file.is_open()) {
      tensor->print(output_file);
      output_file.close();
    } else {
      NNDEPLOY_LOGE("can't open file: %s", filename.c_str());
    }
#endif

    output->set(tensor, index, false);
  }
  // NNDEPLOY_LOGE("infer end!Thread ID: %d.\n", std::this_thread::get_id());
  return status;
}

std::shared_ptr<inference::Inference> Infer::getInference() {
  return inference_;
}

REGISTER_NODE("nndeploy::infer::Infer", Infer);

}  // namespace infer
}  // namespace nndeploy
