#include "nndeploy/detect/drawbox.h"

#include "nndeploy_api_registry.h"

namespace py = pybind11;

namespace nndeploy {
namespace detect {

NNDEPLOY_API_PYBIND11_MODULE("detect", m) {
  // 首先导入dag模块
  py::module::import("nndeploy._nndeploy_internal.dag");

  // 然后再进行DrawBoxNode和YoloMultiConvDrawBoxNode的绑定
  py::class_<DrawBoxNode, dag::Node, std::shared_ptr<DrawBoxNode>>(
      m, "DrawBoxNode")
      .def(py::init<const std::string &, std::initializer_list<dag::Edge *>,
                    std::initializer_list<dag::Edge *>>())
      .def(py::init<const std::string &, std::vector<dag::Edge *>,
                    std::vector<dag::Edge *>>())
      .def("run", &DrawBoxNode::run);

  py::class_<YoloMultiConvDrawBoxNode, dag::Node,
             std::shared_ptr<YoloMultiConvDrawBoxNode>>(
      m, "YoloMultiConvDrawBoxNode")
      .def(py::init<const std::string &, std::initializer_list<dag::Edge *>,
                    std::initializer_list<dag::Edge *>>())
      .def(py::init<const std::string &, std::vector<dag::Edge *>,
                    std::vector<dag::Edge *>>())
      .def("run", &YoloMultiConvDrawBoxNode::run);
}

}  // namespace detect
}  // namespace nndeploy
