
#ifndef _NNDEPLOY_DETECT_DETECT_YOLO_YOLO_MULTI_OUTPUT_H_
#define _NNDEPLOY_DETECT_DETECT_YOLO_YOLO_MULTI_OUTPUT_H_

#include "nndeploy/base/any.h"
#include "nndeploy/base/common.h"
#include "nndeploy/base/glic_stl_include.h"
#include "nndeploy/base/log.h"
#include "nndeploy/base/macro.h"
#include "nndeploy/base/object.h"
#include "nndeploy/base/opencv_include.h"
#include "nndeploy/base/param.h"
#include "nndeploy/base/status.h"
#include "nndeploy/base/string.h"
#include "nndeploy/dag/edge.h"
#include "nndeploy/dag/graph.h"
#include "nndeploy/dag/node.h"
#include "nndeploy/detect/result.h"
#include "nndeploy/device/buffer.h"
#include "nndeploy/device/device.h"
#include "nndeploy/device/memory_pool.h"
#include "nndeploy/device/tensor.h"
#include "nndeploy/infer/infer.h"
#include "nndeploy/preprocess/cvt_resize_norm_trans.h"

namespace nndeploy {
namespace detect {
// 
class NNDEPLOY_CC_API YoloMultiOutputPostParam : public base::Param {
 public:
  float score_threshold_;
  float nms_threshold_;
  float obj_threshold_;
  int num_classes_;
  int model_h_;
  int model_w_;

  int anchors_stride_8[6] = {10, 13, 16, 30, 33, 23};    // [1, 3, 80, 80, 85]
  int anchors_stride_16[6] = {30, 61, 62, 45, 59, 119};  // [1, 3, 40, 40, 85]
  int anchors_stride_32[6] = {116, 90,  156,
                              198, 373, 326};  // [1, 3, 20, 20, 85]

  int version_ = -1;

  using base::Param::serialize;
  virtual base::Status serialize(rapidjson::Value &json,
                                 rapidjson::Document::AllocatorType &allocator);
  using base::Param::deserialize;
  virtual base::Status deserialize(rapidjson::Value &json);
};

class NNDEPLOY_CC_API YoloMultiOutputPostProcess : public dag::Node {
 public:
  YoloMultiOutputPostProcess(const std::string &name) : dag::Node(name) {
    key_ = "nndeploy::detect::YoloMultiOutputPostProcess";
    desc_ = "YOLO multi-output postprocess[device::Tensor->DetectResult]";
    param_ = std::make_shared<YoloMultiOutputPostParam>();
    this->setInputTypeInfo<device::Tensor>();
    this->setOutputTypeInfo<DetectResult>();
  }
  YoloMultiOutputPostProcess(const std::string &name,
                             std::vector<dag::Edge *> inputs,
                             std::vector<dag::Edge *> outputs)
      : dag::Node(name, inputs, outputs) {
    key_ = "nndeploy::detect::YoloMultiOutputPostProcess";
    desc_ = "YOLO multi-output postprocess[device::Tensor->DetectResult]";
    param_ = std::make_shared<YoloMultiOutputPostParam>();
    this->setInputTypeInfo<device::Tensor>();
    this->setOutputTypeInfo<DetectResult>();
  }
  virtual ~YoloMultiOutputPostProcess() {}

  virtual base::Status run();
};

class NNDEPLOY_CC_API YoloMultiOutputGraph : public dag::Graph {
 public:
  YoloMultiOutputGraph(const std::string &name) : dag::Graph(name) {
    key_ = "nndeploy::detect::YoloMultiOutputGraph";
    desc_ = "yolo v5/v6/v7/v8/v11 graph[cv::Mat->preprocess->infer->postprocess->DetectResult]";
    this->setInputTypeInfo<cv::Mat>();
    this->setOutputTypeInfo<DetectResult>();
    pre_ = this->createNode<preprocess::CvtResizeNormTrans>("preprocess");
    if (pre_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create preprocessing node");
      constructed_ = false;
    }
    infer_ = dynamic_cast<infer::Infer *>(
        this->createNode<infer::Infer>("infer"));
    if (infer_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create inference node");
      constructed_ = false;
    }
    post_ = this->createNode<YoloMultiOutputPostProcess>("postprocess");
    if (post_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create postprocessing node");
      constructed_ = false;
    }
  }
  YoloMultiOutputGraph(const std::string &name,
                       std::vector<dag::Edge *> inputs,
                       std::vector<dag::Edge *> outputs)
      : dag::Graph(name, inputs, outputs) {
    key_ = "nndeploy::detect::YoloMultiOutputGraph";
    desc_ = "yolo v5/v6/v7/v8/v11 graph[cv::Mat->preprocess->infer->postprocess->DetectResult]";
    this->setInputTypeInfo<cv::Mat>();
    this->setOutputTypeInfo<DetectResult>();
    pre_ = this->createNode<preprocess::CvtResizeNormTrans>("preprocess");
    if (pre_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create preprocessing node");
      constructed_ = false;
    }
    infer_ = dynamic_cast<infer::Infer *>(
        this->createNode<infer::Infer>("infer"));
    if (infer_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create inference node");
      constructed_ = false;
    }
    post_ = this->createNode<YoloMultiOutputPostProcess>("postprocess");
    if (post_ == nullptr) {
      NNDEPLOY_LOGE("Failed to create postprocessing node");
      constructed_ = false;
    }
  }

  virtual ~YoloMultiOutputGraph() {}

  base::Status defaultParam() {    
    preprocess::CvtResizeNormTransParam *pre_param =
        dynamic_cast<preprocess::CvtResizeNormTransParam *>(pre_->getParam());
    pre_param->src_pixel_type_ = base::kPixelTypeBGR;
    pre_param->dst_pixel_type_ = base::kPixelTypeRGB;
    pre_param->interp_type_ = base::kInterpTypeLinear;
    pre_param->h_ = 640;
    pre_param->w_ = 640;
    
    YoloMultiOutputPostParam *post_param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    post_param->score_threshold_ = 0.7;
    post_param->nms_threshold_ = 0.3;
    post_param->num_classes_ = 80;
    post_param->model_h_ = 640;
    post_param->model_w_ = 640;
    post_param->version_ = 5;

    return base::kStatusCodeOk;
  }
  base::Status make(const dag::NodeDesc &pre_desc,
                    const dag::NodeDesc &infer_desc,
                    base::InferenceType inference_type,
                    const dag::NodeDesc &post_desc) {
    this->setNodeDesc(pre_, pre_desc);
    this->setNodeDesc(infer_, infer_desc);
    this->setNodeDesc(post_, post_desc);
    this->defaultParam();
    base::Status status = infer_->setInferenceType(inference_type);
    if (status != base::kStatusCodeOk) {
      NNDEPLOY_LOGE("Failed to set inference type");
      return status;
    }
    return base::kStatusCodeOk;
  }

  base::Status setInferenceType(base::InferenceType inference_type) {
    base::Status status = infer_->setInferenceType(inference_type);
    if (status != base::kStatusCodeOk) {
      NNDEPLOY_LOGE("Failed to set inference type");
      return status;
    }
    return base::kStatusCodeOk;
  }
  base::Status setInferParam(base::DeviceType device_type,
                             base::ModelType model_type, bool is_path,
                             std::vector<std::string> &model_value) {
    auto param = dynamic_cast<inference::InferenceParam *>(infer_->getParam());
    param->device_type_ = device_type;
    param->model_type_ = model_type;
    param->is_path_ = is_path;
    param->model_value_ = model_value;
    return base::kStatusCodeOk;
  }

  /**
   * @brief Set preprocessing parameters
   * @param pixel_type Input image pixel format (e.g. RGB, BGR)
   * @return kStatusCodeOk on success
   */
  base::Status setSrcPixelType(base::PixelType pixel_type) {
    preprocess::CvtResizeNormTransParam *param =
        dynamic_cast<preprocess::CvtResizeNormTransParam *>(pre_->getParam());
    param->src_pixel_type_ = pixel_type;
    return base::kStatusCodeOk;
  }

  base::Status setScoreThreshold(float score_threshold) {
    YoloMultiOutputPostParam *param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    param->score_threshold_ = score_threshold;
    return base::kStatusCodeOk;
  }

  base::Status setNmsThreshold(float nms_threshold) {
    YoloMultiOutputPostParam *param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    param->nms_threshold_ = nms_threshold;
    return base::kStatusCodeOk;
  }

  base::Status setNumClasses(int num_classes) {
    YoloMultiOutputPostParam *param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    param->num_classes_ = num_classes;
    return base::kStatusCodeOk;
  }

  base::Status setModelHW(int model_h, int model_w) {
    YoloMultiOutputPostParam *param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    param->model_h_ = model_h;
    param->model_w_ = model_w;
    return base::kStatusCodeOk;
  }

  base::Status setVersion(int version) {
    YoloMultiOutputPostParam *param =
        dynamic_cast<YoloMultiOutputPostParam *>(post_->getParam());
    param->version_ = version;
    return base::kStatusCodeOk;
  }

  std::vector<dag::Edge *> forward(std::vector<dag::Edge *> inputs) {
    std::vector<dag::Edge *> pre_outputs = (*pre_)(inputs);
    std::vector<dag::Edge *> infer_outputs = (*infer_)(pre_outputs);
    std::vector<dag::Edge *> post_outputs = (*post_)(infer_outputs);
    return post_outputs;
  }

 private:
  dag::Node *pre_ = nullptr;       ///< Preprocessing node pointer
  infer::Infer *infer_ = nullptr;  ///< Inference node pointer
  dag::Node *post_ = nullptr;      ///< Postprocessing node pointer
};

}  // namespace detect
}  // namespace nndeploy

#endif /* _NNDEPLOY_DETECT_DETECT_YOLO_YOLO_MULTI_OUTPUT_H_ */
