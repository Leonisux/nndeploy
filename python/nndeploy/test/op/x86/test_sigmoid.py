import unittest
import numpy as np
import torch
import nndeploy
from nndeploy.op import functional as F
from nndeploy.base import name_to_device_type_code
from nndeploy.device.tensor import (
    create_tensor_from_numpy,
    create_numpy_from_tensor,
)


class TestSigmoidOp(unittest.TestCase):

    def test_sigmoid(self):
        src_shape = [16, 1000]

        np_src = np.random.uniform(-5, 5, src_shape).astype(np.float32)

        torch_result = torch.sigmoid(
            torch.from_numpy(np_src)
        )

        src = create_tensor_from_numpy(np_src)

        x86_src = src.to(nndeploy.base.DeviceType("x86"))

        x86_result = F.sigmoid(x86_src)

        nndeploy_result = x86_result.to(nndeploy.base.DeviceType("cpu"))

        self.assertTrue(
            np.allclose(
                torch_result.detach().numpy(),  
                create_numpy_from_tensor(nndeploy_result), 
                rtol=1e-02,  
                atol=1e-04, 
            ),
        )


if __name__ == "__main__":
    unittest.main()