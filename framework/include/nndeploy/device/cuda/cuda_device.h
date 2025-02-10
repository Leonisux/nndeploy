#ifndef _NNDEPLOY_DEVICE_CUDA_DEVICE_H_
#define _NNDEPLOY_DEVICE_CUDA_DEVICE_H_

#include "nndeploy/device/cuda/cuda_include.h"
#include "nndeploy/device/device.h"

namespace nndeploy {
namespace device {

class CudaArchitecture : public Architecture {
 public:
  /**
   * @brief Construct a new Cuda Architecture object
   *
   * @param device_type_code
   */
  explicit CudaArchitecture(base::DeviceTypeCode device_type_code);

  /**
   * @brief Destroy the Cuda Architecture object
   *
   */
  virtual ~CudaArchitecture();

  /**
   * @brief Check whether the device corresponding to the current device id
   * exists, mainly serving GPU devices
   *
   * @param device_id - device id
   * @param library_path - Mainly serving OpenCL, using the OpenCL dynamic
   * library provided by the user
   * @return base::Status
   */
  virtual base::Status checkDevice(int device_id = 0,
                                   std::string library_path = "") override;

  /**
   * @brief Enable the device corresponding to the current device id，mainly
   * serving GPU devices
   *
   * @param device_id - device id
   * @param library_path - Mainly serving OpenCL, using the OpenCL dynamic
   * library provided by the user
   * @return base::Status
   */
  virtual base::Status enableDevice(int device_id = 0,
                                    std::string library_path = "") override;

  /**
   * @brief Get the Device object
   *
   * @param device_id
   * @return Device*
   */
  virtual Device *getDevice(int device_id) override;

  /**
   * @brief Get the Device Info object
   *
   * @param library_path
   * @return std::vector<DeviceInfo>
   */
  virtual std::vector<DeviceInfo> getDeviceInfo(
      std::string library_path = "") override;
};

class CudaStreamWrapper {
 public:
  void *external_command_queue_ = nullptr;
  cudaStream_t stream_;
};

/**
 * @brief
 *
 */
class NNDEPLOY_CC_API CudaDevice : public Device {
  /**
   * @brief friend class
   *
   */
  friend class CudaArchitecture;

 public:
  virtual BufferDesc toBufferDesc(const TensorDesc &desc,
                                  const base::IntVector &config);

  virtual void *allocate(size_t size);
  virtual void *allocate(const BufferDesc &desc);

  virtual void deallocate(void *ptr);

  virtual base::Status copy(void *src, void *dst, size_t size,
                            Stream *stream = nullptr) override;
  virtual base::Status download(void *src, void *dst, size_t size,
                                Stream *stream = nullptr) override;
  virtual base::Status upload(void *src, void *dst, size_t size,
                              Stream *stream = nullptr) override;

  virtual base::Status copy(Buffer *src, Buffer *dst,
                            Stream *stream = nullptr) override;
  virtual base::Status download(Buffer *src, Buffer *dst,
                                Stream *stream = nullptr) override;
  virtual base::Status upload(Buffer *src, Buffer *dst,
                              Stream *stream = nullptr) override;

  // TODO: map/unmap
  // virtual Buffer* map(Buffer* src);
  // virtual base::Status unmap(Buffer* src, Buffer* dst);
  // TODO: share? opencl / vpu / hvx?
  // virtual Buffer* share(Buffer* src);
  // virtual base::Status unshare(Buffer* src, Buffer* dst);

  virtual void *getContext();

  virtual int newCommandQueue();
  virtual base::Status deleteCommandQueue(int index);
  virtual base::Status deleteCommandQueue(void *stream);
  virtual int setCommandQueue(void *stream, bool is_external = true);

  virtual void *getCommandQueue(int index = 0);

  virtual base::Status synchronize(int index = 0);

 public:
  /**
   * @brief Construct a new Cuda Device object
   *
   * @param device_type
   * @param stream
   * @param library_path
   */
  CudaDevice(base::DeviceType device_type, std::string library_path = "")
      : Device(device_type) {
    CudaStreamWrapper cuda_stream_wrapper;
    cuda_stream_wrapper.external_command_queue_ = nullptr;
    cuda_stream_wrapper.stream_ = nullptr;
    insertStream(stream_index_, cuda_stream_wrapper);
  };
  /**
   * @brief Destroy the Cuda Device object
   *
   */
  virtual ~CudaDevice() {};

  /**
   * @brief init
   *
   * @return base::Status
   */
  virtual base::Status init();
  /**
   * @brief deinit
   *
   * @return base::Status
   */
  virtual base::Status deinit();

 private:
  int stream_index_ = 0;
  std::map<int, CudaStreamWrapper> cuda_stream_wrapper_;
};

class CudaStream : public Stream {
 public:
  CudaStream(Device *device, void *stream = nullptr)
      : Stream(device, stream), stream_((cudaStream_t)stream) {}
  virtual ~CudaStream() {}

  cudaStream_t getStream() { return stream_; }

 private:
  cudaStream_t stream_;
};

}  // namespace device
}  // namespace nndeploy

#endif
