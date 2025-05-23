#include "nndeploy/op/op_conv.h"

#include "aclnnop/aclnn_convolution.h"
#include "aclnnop/aclnn_permute.h"
#include "ascend_c/op_conv_kernel.h"
#include "nndeploy/op/ascend_cl/op_convert.h"
#include "nndeploy/op/ascend_cl/op_include.h"
#include "nndeploy/op/ascend_cl/op_util.h"
#include "nndeploy/op/op.h"

namespace nndeploy {
namespace op {

#if defined(ENABLE_NNDEPLOY_OP_ASCEND_C) && \
    defined(ENABLE_NNDEPLOY_OP_ASCEND_C_CONV2D)

#include "acl/acl.h"
#include "aclrtlaunch_conv2d.h"
class AscendCLOpConv : public OpConv {
 public:
  AscendCLOpConv() {}
  virtual ~AscendCLOpConv() {}

  /**
   * @brief Initialize the convolution operator
   * @details Performs the following initialization steps:
   *   1. Get and validate convolution parameters
   *   2. Set up computation stream
   *   3. Process input and weight tensors
   *   4. Calculate tiling data
   *   5. Validate kernel shape(3x3), dilation(1x1) and padding(0, 0, 0, 0)
   * @return base::Status Initialization status
   */
  virtual base::Status init() {
    base::Status status = Op::init();
    if (status != base::kStatusCodeOk) {
      return status;
    }
    // Get convolution parameters
    ir::ConvParam *param = (ir::ConvParam *)op_desc_.op_param_.get();

    // Get device stream
    device::Device *device = device::getDevice(device_type_);
    inner_stream_ =
        (aclrtStream)stream_->as<device::AscendCLStream>()->getStream();

    // Process input feature map tensor
    if (device::isHostDeviceType(inputs_[0]->getDeviceType())) {
      inner_fm_input_ = new device::Tensor(device, inputs_[0]->getDesc(),
                                           inputs_[0]->getName());
      inputs_[0]->copyTo(inner_fm_input_);
      inner_output_ = outputs_[0];
    } else {
      inner_fm_input_ = inputs_[0];
      inner_output_ = outputs_[0];
    }

    if (device::isHostDeviceType(inputs_[1]->getDeviceType())) {
      inner_we_input_ = new device::Tensor(device, inputs_[1]->getDesc(),
                                           inputs_[1]->getName());
      inputs_[1]->copyTo(inner_we_input_);
    } else {
      inner_we_input_ = inputs_[1];
    }

    base::IntVector input_shape = inner_fm_input_->getShape();
    base::IntVector weight_shape = inner_we_input_->getShape();
    tiling_data_.batchSize = input_shape[0];
    tiling_data_.inChannel = input_shape[1];
    tiling_data_.inHeight = input_shape[2];
    tiling_data_.inWidth = input_shape[3];
    tiling_data_.outChannel = weight_shape[0];
    tiling_data_.stride = param->strides_[0];
    tiling_data_.kernelSize = param->kernel_shape_[0];
    tiling_data_.outHeight =
        (tiling_data_.inHeight + param->pads_[0] + param->pads_[1] -
         param->dilations_[0] * (param->kernel_shape_[0] - 1) - 1) /
            param->strides_[0] +
        1;
    tiling_data_.outWidth =
        (tiling_data_.inWidth + param->pads_[2] + param->pads_[3] -
         param->dilations_[1] * (param->kernel_shape_[1] - 1) - 1) /
            param->strides_[1] +
        1;
    tiling_data_.coreNum = 1;

    base::DataType data_type = inner_fm_input_->getDataType();
    if (data_type.code_ == base::kDataTypeCodeFp) {
      if (data_type.bits_ == 16) {
        tiling_data_.dataType = 0;
      } else {
        tiling_data_.dataType = 1;
      }
    } else {
      NNDEPLOY_LOGE("Conv only support half and float32 data type.");
      return base::kStatusCodeErrorInvalidParam;
    }

    // support attr
    if (param->kernel_shape_.size() == 2) {
      if (param->kernel_shape_[0] != 3 || param->kernel_shape_[1] != 3) {
        NNDEPLOY_LOGE("not support kernel shape: %d, %d",
                      param->kernel_shape_[0], param->kernel_shape_[1]);
        return base::kStatusCodeErrorOpAscendCL;
      }
    } else {
      NNDEPLOY_LOGE("not support shape size: %d", param->kernel_shape_.size());
      return base::kStatusCodeErrorOpAscendCL;
    }

    if (param->dilations_[0] != 1 || param->dilations_[1] != 1) {
      NNDEPLOY_LOGE("not support dilation: %d, %d", param->dilations_[0],
                    param->dilations_[1]);
      return base::kStatusCodeErrorOpAscendCL;
    }

    if (param->pads_[0] != 0 || param->pads_[1] != 0 || param->pads_[2] != 0 ||
        param->pads_[3] != 0) {
      NNDEPLOY_LOGE("not support padding: %d, %d, %d, %d", param->pads_[0],
                    param->pads_[1], param->pads_[2], param->pads_[3]);
      return base::kStatusCodeErrorOpAscendCL;
    }

    return base::kStatusCodeOk;
  }

  /**
   * @brief Release operator resources
   * @details Free internally allocated tensors and resources
   * @return base::Status Deinitialization status
   */
  virtual base::Status deinit() {
    if (inner_fm_input_ != nullptr) {
      if (device::isHostDeviceType(inputs_[0]->getDeviceType())) {
        delete inner_fm_input_;
        inner_fm_input_ = nullptr;
      }
    }
    if (inner_we_input_ != nullptr) {
      if (device::isHostDeviceType(inputs_[1]->getDeviceType())) {
        delete inner_we_input_;
        inner_we_input_ = nullptr;
      }
    }
    return Op::deinit();
  }

  /**
   * @brief Prepare for execution
   * @details Allocate device memory for tiling data and perform data transfer
   * @return base::Status Preparation status
   */
  virtual base::Status preRun() {
    tiling_ = &tiling_data_;

    device::Device *device = device::getDevice(device_type_);
    if (acl_inner_fm_input_ == nullptr) {
      acl_inner_fm_input_ =
          AscendCLOpConvert::convertFromTensor(inner_fm_input_, ACL_FORMAT_ND);
    }

    if (acl_inner_we_input_ == nullptr) {
      acl_inner_we_input_ =
          AscendCLOpConvert::convertFromTensor(inner_we_input_, ACL_FORMAT_ND);
    }

    base::DataType data_type = inner_fm_input_->getDataType();
    aclDataType acl_data_type = ACL_FLOAT16;
    if (tiling_data_.dataType == 0) {
      acl_data_type = ACL_FLOAT16;
    } else if (tiling_data_.dataType == 1) {
      acl_data_type = ACL_FLOAT;
    } else {
      return base::kStatusCodeErrorInvalidParam;
    }

    if (acl_inner_buf_0_ == nullptr) {
      std::vector<int64_t> shape = {tiling_data_.batchSize,
                                    tiling_data_.inHeight, tiling_data_.inWidth,
                                    tiling_data_.inChannel};
      int64_t input_size = getAclOpShapeSize(shape);
      std::vector<int64_t> strides = getAclOpStrides(shape);
      CHECK_ACLNN_STATUS(aclrtMalloc((void **)&inner_buf_0_,
                                     input_size * data_type.size(),
                                     ACL_MEM_MALLOC_HUGE_FIRST));
      acl_inner_buf_0_ = aclCreateTensor(
          shape.data(), shape.size(), acl_data_type, strides.data(), 0,
          ACL_FORMAT_ND, shape.data(), shape.size(), inner_buf_0_);
    }

    if (acl_inner_buf_1_ == nullptr) {
      std::vector<int64_t> shape = {
          tiling_data_.outChannel, tiling_data_.kernelSize,
          tiling_data_.kernelSize, tiling_data_.inChannel};
      int64_t input_size = getAclOpShapeSize(shape);
      std::vector<int64_t> strides = getAclOpStrides(shape);
      CHECK_ACLNN_STATUS(aclrtMalloc((void **)&inner_buf_1_,
                                     input_size * data_type.size(),
                                     ACL_MEM_MALLOC_HUGE_FIRST));
      acl_inner_buf_1_ = aclCreateTensor(
          shape.data(), shape.size(), acl_data_type, strides.data(), 0,
          ACL_FORMAT_ND, shape.data(), shape.size(), inner_buf_1_);
    }

    if (acl_inner_buf_2_ == nullptr) {
      std::vector<int64_t> shape = {
          tiling_data_.batchSize, tiling_data_.outHeight, tiling_data_.outWidth,
          tiling_data_.outChannel};
      int64_t output_size = getAclOpShapeSize(shape);
      std::vector<int64_t> strides = getAclOpStrides(shape);
      CHECK_ACLNN_STATUS(aclrtMalloc((void **)&inner_buf_2_,
                                     output_size * data_type.size(),
                                     ACL_MEM_MALLOC_HUGE_FIRST));
      acl_inner_buf_2_ = aclCreateTensor(
          shape.data(), shape.size(), acl_data_type, strides.data(), 0,
          ACL_FORMAT_ND, shape.data(), shape.size(), inner_buf_2_);
    }

    if (acl_inner_output_ == nullptr) {
      acl_inner_output_ =
          AscendCLOpConvert::convertFromTensor(inner_output_, ACL_FORMAT_ND);
    }

    return base::kStatusCodeOk;
  }

  /**
   * @brief Execute convolution computation
   * @details Call Ascend CL convolution kernel to perform computation
   * @return base::Status Execution status
   */
  virtual base::Status run() {
    // transpose input data to NHWC format
    std::vector<int64_t> input_dims_data = {0, 2, 3, 1};
    input_dims =
        aclCreateIntArray(input_dims_data.data(), input_dims_data.size());
    CHECK_ACLNN_STATUS(aclnnPermuteGetWorkspaceSize(
        acl_inner_fm_input_, input_dims, acl_inner_buf_0_, &workspace_size_,
        &executor_));

    void *input_workspace = nullptr;
    if (workspace_size_ > 0) {
      input_workspace =
          device::getDevice(device_type_)->allocate(workspace_size_);
    }
    CHECK_ACLNN_STATUS(aclnnPermute(input_workspace, workspace_size_, executor_,
                                    inner_stream_));
    CHECK_ACLNN_STATUS(aclrtSynchronizeStream(inner_stream_));

    if (workspace_size_ > 0 && input_workspace != nullptr) {
      aclrtFree(input_workspace);
      input_workspace = nullptr;
    }

    // transpose kernel data to NHWC format
    std::vector<int64_t> kernel_dims_data = {0, 2, 3, 1};
    kernel_dims =
        aclCreateIntArray(kernel_dims_data.data(), kernel_dims_data.size());
    CHECK_ACLNN_STATUS(aclnnPermuteGetWorkspaceSize(
        acl_inner_we_input_, kernel_dims, acl_inner_buf_1_, &workspace_size_,
        &executor_));

    void *kernel_workspace = nullptr;
    if (workspace_size_ > 0) {
      kernel_workspace =
          device::getDevice(device_type_)->allocate(workspace_size_);
    }
    CHECK_ACLNN_STATUS(aclnnPermute(kernel_workspace, workspace_size_,
                                    executor_, inner_stream_));
    CHECK_ACLNN_STATUS(aclrtSynchronizeStream(inner_stream_));

    if (workspace_size_ > 0 && kernel_workspace != nullptr) {
      aclrtFree(kernel_workspace);
      kernel_workspace = nullptr;
    }

    ACLRT_LAUNCH_KERNEL(conv2d)
    (tiling_data_.coreNum, inner_stream_, (uint8_t *)inner_buf_0_,
     (uint8_t *)inner_buf_1_, (uint8_t *)inner_buf_2_,
     reinterpret_cast<Conv2dTilingData *>(tiling_));
    CHECK_ACLNN_STATUS(aclrtSynchronizeStream(inner_stream_));

    // transpose output data to NCHW format
    std::vector<int64_t> output_dims_data = {0, 3, 1, 2};
    output_dims =
        aclCreateIntArray(output_dims_data.data(), output_dims_data.size());
    CHECK_ACLNN_STATUS(aclnnPermuteGetWorkspaceSize(
        acl_inner_buf_2_, output_dims, acl_inner_output_, &workspace_size_,
        &executor_));

    void *output_workspace = nullptr;
    if (workspace_size_ > 0) {
      output_workspace =
          device::getDevice(device_type_)->allocate(workspace_size_);
    }
    CHECK_ACLNN_STATUS(aclnnPermute(output_workspace, workspace_size_,
                                    executor_, inner_stream_));
    CHECK_ACLNN_STATUS(aclrtSynchronizeStream(inner_stream_));

    if (workspace_size_ > 0 && output_workspace != nullptr) {
      aclrtFree(output_workspace);
      output_workspace = nullptr;
    }
    return base::kStatusCodeOk;
  }

  /**
   * @brief Post-execution cleanup
   * @details Release device memory used for tiling data
   * @return base::Status Cleanup status
   */
  virtual base::Status postRun() {
    if (input_dims != nullptr) {
      aclDestroyIntArray(input_dims);
      input_dims = nullptr;
    }
    if (kernel_dims != nullptr) {
      aclDestroyIntArray(kernel_dims);
      kernel_dims = nullptr;
    }
    if (output_dims != nullptr) {
      aclDestroyIntArray(output_dims);
      output_dims = nullptr;
    }
    if (acl_inner_fm_input_ != nullptr) {
      aclDestroyTensor(acl_inner_fm_input_);
      acl_inner_fm_input_ = nullptr;
    }
    if (acl_inner_we_input_ != nullptr) {
      aclDestroyTensor(acl_inner_we_input_);
      acl_inner_we_input_ = nullptr;
    }
    if (acl_inner_output_ != nullptr) {
      aclDestroyTensor(acl_inner_output_);
      acl_inner_output_ = nullptr;
    }
    if (inner_buf_0_ != nullptr) {
      if (acl_inner_buf_0_ != nullptr) {
        aclDestroyTensor(acl_inner_buf_0_);
        acl_inner_buf_0_ = nullptr;
      }
      aclrtFree(inner_buf_0_);
      inner_buf_0_ = nullptr;
    }
    if (inner_buf_1_ != nullptr) {
      if (acl_inner_buf_1_ != nullptr) {
        aclDestroyTensor(acl_inner_buf_1_);
        acl_inner_buf_1_ = nullptr;
      }
      aclrtFree(inner_buf_1_);
      inner_buf_1_ = nullptr;
    }
    if (inner_buf_2_ != nullptr) {
      if (acl_inner_buf_2_ != nullptr) {
        aclDestroyTensor(acl_inner_buf_2_);
        acl_inner_buf_2_ = nullptr;
      }
      aclrtFree(inner_buf_2_);
      inner_buf_2_ = nullptr;
    }
    if (executor_ != nullptr) {
      executor_ = nullptr;
    }
    return base::kStatusCodeOk;
  }

 private:
  // Operator type identifier
  std::string inner_op_type_ = "Conv2d";

  // Ascend CL computation stream
  aclrtStream inner_stream_ = nullptr;

  // Internal tensors for input feature map and weights
  device::Tensor *inner_fm_input_ = nullptr;
  device::Tensor *inner_we_input_ = nullptr;
  device::Tensor *inner_output_ = nullptr;

  void *inner_buf_0_ = nullptr;
  void *inner_buf_1_ = nullptr;
  void *inner_buf_2_ = nullptr;

  aclTensor *acl_inner_fm_input_ = nullptr;
  aclTensor *acl_inner_we_input_ = nullptr;
  aclTensor *acl_inner_output_ = nullptr;
  aclTensor *acl_inner_buf_0_ = nullptr;
  aclTensor *acl_inner_buf_1_ = nullptr;
  aclTensor *acl_inner_buf_2_ = nullptr;

  aclIntArray *input_dims = nullptr;
  aclIntArray *kernel_dims = nullptr;
  aclIntArray *output_dims = nullptr;

  aclOpExecutor *executor_ = nullptr;

  // Tiling related data
  void *tiling_ = nullptr;        // Device-side tiling data
  Conv2dTilingData tiling_data_;  // Host-side tiling data
};
#else
class AscendCLOpConv : public OpConv {
 public:
  AscendCLOpConv() {}
  virtual ~AscendCLOpConv() {}

  virtual base::Status init() {
    // 参数
    ir::ConvParam *param = (ir::ConvParam *)op_desc_.op_param_.get();
    stride_ = AscendCLOpConvert::convertFromIntVector(param->strides_);
    padding_ = AscendCLOpConvert::convertFromIntVector(param->pads_);
    dilation_ = AscendCLOpConvert::convertFromIntVector(param->dilations_);
    transposed_ = false;
    base::IntVector output_pads = {0, 0, 0, 0};
    output_padding_ = AscendCLOpConvert::convertFromIntVector(output_pads);
    groups_ = param->group_;
    cube_math_type_ = 0;

    base::Status status = Op::init();
    if (status != base::kStatusCodeOk) {
      return status;
    }
    device::Device *device = device::getDevice(device_type_);
    inner_stream_ =
        (aclrtStream)stream_->as<device::AscendCLStream>()->getStream();

    // 权重
    if (weight_ == nullptr) {
      weight_ = new device::Tensor(device, inputs_[1]->getDesc(),
                                   inputs_[1]->getName());
      inputs_[1]->copyTo(weight_);
    }
    if (weight_->getShape().size() == 3) {
      dst_data_format_ = ACL_FORMAT_NCL;
    } else if (weight_->getShape().size() == 4) {
      dst_data_format_ = ACL_FORMAT_NCHW;
    } else if (weight_->getShape().size() == 5) {
      dst_data_format_ = ACL_FORMAT_NCDHW;
    } else {
      NNDEPLOY_LOGE("not support shape size: %d", weight_->getShape().size());
      return base::kStatusCodeErrorOpAscendCL;
    }
    if (transposed_ == true) {
      dst_data_format_ = ACL_FORMAT_ND;
    }
    if (inner_weight_ == nullptr) {
      inner_weight_ =
          AscendCLOpConvert::convertFromTensor(weight_, dst_data_format_);
    }

    // 偏置
    if (inputs_.size() > 2) {
      if (bias_ == nullptr) {
        bias_ = new device::Tensor(device, inputs_[2]->getDesc(),
                                   inputs_[2]->getName());
        inputs_[2]->copyTo(bias_);
      }
      if (inner_bias_ == nullptr) {
        inner_bias_ =
            AscendCLOpConvert::convertFromTensor(bias_, ACL_FORMAT_ND);
      }
    }
#if 0
    if (op_desc_.name_ == "/model.0/conv/Conv") {
      NNDEPLOY_LOGI("strides: %d, %d\n", param->strides_[0],
                    param->strides_[1]);
      NNDEPLOY_LOGI("pads: %d, %d, %d, %d\n", param->pads_[0],
      param->pads_[1],
                    param->pads_[2], param->pads_[3]);
      NNDEPLOY_LOGI("dilations: %d, %d\n", param->dilations_[0],
                    param->dilations_[1]);
      NNDEPLOY_LOGI("group: %d\n", param->group_);
      weight_->getDesc().print();
      bias_->getDesc().print();
    }
#endif
    return base::kStatusCodeOk;
  }
  virtual base::Status deinit() {
    if (stride_ != nullptr) {
      aclDestroyIntArray(stride_);
      stride_ = nullptr;
    }
    if (padding_ != nullptr) {
      aclDestroyIntArray(padding_);
      padding_ = nullptr;
    }
    if (dilation_ != nullptr) {
      aclDestroyIntArray(dilation_);
      dilation_ = nullptr;
    }
    if (output_padding_ != nullptr) {
      aclDestroyIntArray(output_padding_);
      output_padding_ = nullptr;
    }
    if (inner_weight_ != nullptr) {
      aclDestroyTensor(inner_weight_);
      inner_weight_ = nullptr;
    }
    if (inner_bias_ != nullptr) {
      aclDestroyTensor(inner_bias_);
      inner_bias_ = nullptr;
    }
    if (weight_ != nullptr) {
      delete weight_;
      weight_ = nullptr;
    }
    if (bias_ != nullptr) {
      delete bias_;
      bias_ = nullptr;
    }
    return Op::deinit();
  }
  virtual base::Status preRun() {
    // 输入输出
    if (inner_input_ == nullptr) {
      inner_input_ =
          AscendCLOpConvert::convertFromTensor(inputs_[0], dst_data_format_);
    }
    if (inner_output_ == nullptr) {
      inner_output_ =
          AscendCLOpConvert::convertFromTensor(outputs_[0], dst_data_format_);
    }
    // 创建算子
    if (executor_ == nullptr) {
      aclnnStatus aclnn_status = aclnnConvolutionGetWorkspaceSize(
          inner_input_, inner_weight_, inner_bias_, stride_, padding_,
          dilation_, transposed_, output_padding_, groups_, inner_output_,
          cube_math_type_, &workspace_size_, &executor_);
      if (aclnn_status != ACL_SUCCESS) {
        NNDEPLOY_LOGE(
            "aclnnConvolutionGetWorkspaceSize failed, error code: %d.\n",
            aclnn_status);
        return base::kStatusCodeErrorOpAscendCL;
      }
    }
    return base::kStatusCodeOk;
  }
  virtual base::Status run() {
    aclnnStatus aclnn_status =
        aclnnConvolution(workspace_, workspace_size_, executor_, inner_stream_);
    if (aclnn_status != ACL_SUCCESS) {
      NNDEPLOY_LOGE("aclnnConvolution failed, error code: %d.\n", aclnn_status);
      return base::kStatusCodeErrorOpAscendCL;
    }
    return base::kStatusCodeOk;
  }
  virtual base::Status postRun() {
    if (inner_input_ != nullptr) {
      aclDestroyTensor(inner_input_);
      inner_input_ = nullptr;
    }
    if (inner_output_ != nullptr) {
      aclDestroyTensor(inner_output_);
      inner_output_ = nullptr;
    }
    if (executor_ != nullptr) {
      executor_ = nullptr;
    }
    return base::kStatusCodeOk;
  }

 private:
  std::string inner_op_type_ = "Convolution";

  aclFormat dst_data_format_ = ACL_FORMAT_NCHW;

  aclTensor *inner_input_ = nullptr;
  aclTensor *inner_weight_ = nullptr;
  aclTensor *inner_bias_ = nullptr;
  aclIntArray *stride_ = nullptr;
  aclIntArray *padding_ = nullptr;
  aclIntArray *dilation_ = nullptr;
  bool transposed_ = false;
  aclIntArray *output_padding_ = nullptr;
  int64_t groups_ = 1;
  aclTensor *inner_output_ = nullptr;
  int8_t cube_math_type_ = 0;

  aclOpExecutor *executor_ = nullptr;

  aclrtStream inner_stream_ = nullptr;
  aclopAttr *attr_ = nullptr;

  device::Tensor *weight_ = nullptr;
  device::Tensor *bias_ = nullptr;
};
#endif

REGISTER_OP_IMPLEMENTION(kDeviceTypeCodeAscendCL, ir::kOpTypeConv,
                         AscendCLOpConv)

}  // namespace op
}  // namespace nndeploy
