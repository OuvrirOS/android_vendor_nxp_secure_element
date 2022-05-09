/******************************************************************************
 *
 *  Copyright 2020 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#define LOG_TAG "nxpese@1.0-service"
#include <android/hardware/secure_element/1.2/ISecureElement.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>
#include <vendor/nxp/nxpese/1.0/INxpEse.h>
#include <vendor/nxp/eventprocessor/1.0/INxpEseEvtProcessor.h>

#include "NxpEse.h"
#include "NxpEseEvtProcessor.h"
#include "SecureElement.h"
#include "StateMachine.h"
#include "ese_config.h"
#include "SpiEseUpdater.h"

// Generated HIDL files
using android::OK;
using android::sp;
using android::status_t;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::secure_element::V1_2::ISecureElement;
using android::hardware::secure_element::V1_2::implementation::SecureElement;
using vendor::nxp::nxpese::V1_0::INxpEse;
using vendor::nxp::nxpese::V1_0::implementation::NxpEse;
using vendor::nxp::eventprocessor::V1_0::INxpEseEvtProcessor;
using vendor::nxp::eventprocessor::V1_0::implementation::NxpEseEvtProcessor;

class EseUpdateCompletedCallback
    : public SpiEseUpdater::IEseUpdateCompletedCallback {
public:
  void updateEseUpdateState(ESE_UPDATE_STATE evt, void *context) {
    (void)evt;
    ALOGD("%s: enter", __func__);
    if (evt == ESE_UPDATE_COMPLETED) {
      SecureElement::reInitSeService(
          reinterpret_cast<sp<ISecureElement> &>(context));
    }
    return;
  }
  ~EseUpdateCompletedCallback(){};
};

std::shared_ptr<SpiEseUpdater::IEseUpdateCompletedCallback>
    gpEseUpdateCompletedCallback = nullptr;

int main() {
  ALOGD("Initializing State Machine...");
  StateMachine::GetInstance().ProcessExtEvent(EVT_SPI_HW_SERVICE_START);

  ALOGD("Registering SecureElement HALIMPL Service v1.2...");
  sp<ISecureElement> se_service = new SecureElement();
  configureRpcThreadpool(2, true /*callerWillJoin*/);
  spiEseUpdater.checkIfEseClientUpdateReqd();

  std::string spiTermName;
  spiTermName = EseConfig::getString(NAME_NXP_SPI_TERMINAL_NAME, "eSE1");
  ALOGD("Registering SPI interface as %s", spiTermName.c_str());
  status_t status = se_service->registerAsService(spiTermName.c_str());
  if (status != OK) {
    LOG_ALWAYS_FATAL(
        "Could not register service for Secure Element HAL Iface (%d).",
        status);
    return -1;
  }

  ALOGD("Registering SecureElement HALIOCTL Service v1.0...");
  sp<INxpEse> nxp_se_service = new NxpEse();
  status = nxp_se_service->registerAsService();
  if (status != OK) {
    LOG_ALWAYS_FATAL(
        "Could not register service for Power Secure Element Extn Iface (%d).",
        status);
    return -1;
  }

  ALOGD("Registering SecureElement Event Handler Service v1.0...");
  sp<INxpEseEvtProcessor> nxp_se_evt_handler_service = new NxpEseEvtProcessor();
  status = nxp_se_evt_handler_service->registerAsService();
  if (status != OK) {
    LOG_ALWAYS_FATAL(
        "Could not register service for SecureElement Event Handler Extn Iface (%d).",
        status);
    return -1;
  }

  ALOGD("Secure Element HAL Service is ready");
  gpEseUpdateCompletedCallback = std::make_shared<EseUpdateCompletedCallback>();
  spiEseUpdater.doEseUpdateIfReqd(gpEseUpdateCompletedCallback,
                                  static_cast<void *>(se_service.get()));
  joinRpcThreadpool();
  return 1;
}
