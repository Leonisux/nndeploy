
#ifndef _NNDEPLOY_FRAMEWORK_H_
#define _NNDEPLOY_FRAMEWORK_H_

#include "nndeploy/base/macro.h"

extern NNDEPLOY_CC_API std::string nndeployGetVersion();

/**
 * @brief init framework
 *
 * @return int
 * @retval 0 success
 * @retval other failed
 */
extern NNDEPLOY_CC_API int nndeployFrameworkInit();

/**
 * @brief deinit framework
 *
 * @return int
 * @retval 0 success
 * @retval other failed
 */
extern NNDEPLOY_CC_API int nndeployFrameworkDeinit();

#endif
