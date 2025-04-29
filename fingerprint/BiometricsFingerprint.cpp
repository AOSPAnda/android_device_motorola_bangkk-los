/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "android.hardware.biometrics.fingerprint@2.3-service.bangkk"

#include "BiometricsFingerprint.h"

#include <android-base/logging.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <chrono>
#include <thread>

#include <display/drm/sde_drm.h>

namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace V2_3 {
namespace implementation {

BiometricsFingerprint::BiometricsFingerprint() {
    mFingerprintV2_1 = IBiometricsFingerprint_2_1::getService();
    mFingerprintRbs = IBiometricsFingerprintRbs::getService();
}

Return<uint64_t> BiometricsFingerprint::setNotify(
        const sp<IBiometricsFingerprintClientCallback>& clientCallback) {
    return mFingerprintV2_1->setNotify(clientCallback);
}

Return<uint64_t> BiometricsFingerprint::preEnroll() {
    return mFingerprintV2_1->preEnroll();
}

Return<RequestStatus> BiometricsFingerprint::enroll(const hidl_array<uint8_t, 69>& hat,
                                                    uint32_t gid, uint32_t timeoutSec) {
    return mFingerprintV2_1->enroll(hat, gid, timeoutSec);
}

Return<RequestStatus> BiometricsFingerprint::postEnroll() {
    return mFingerprintV2_1->postEnroll();
}

Return<uint64_t> BiometricsFingerprint::getAuthenticatorId() {
    return mFingerprintV2_1->getAuthenticatorId();
}

Return<RequestStatus> BiometricsFingerprint::cancel() {
    setHbmState(HbmState::HBM_OFF);
    return mFingerprintV2_1->cancel();
}

Return<RequestStatus> BiometricsFingerprint::enumerate() {
    return mFingerprintV2_1->enumerate();
}

Return<RequestStatus> BiometricsFingerprint::remove(uint32_t gid, uint32_t fid) {
    return mFingerprintV2_1->remove(gid, fid);
}

Return<RequestStatus> BiometricsFingerprint::setActiveGroup(uint32_t gid,
                                                            const hidl_string& storePath) {
    return mFingerprintV2_1->setActiveGroup(gid, storePath);
}

Return<RequestStatus> BiometricsFingerprint::authenticate(uint64_t operationId, uint32_t gid) {
    setHbmState(HbmState::HBM_OFF);
    return mFingerprintV2_1->authenticate(operationId, gid);
}

Return<bool> BiometricsFingerprint::isUdfps(uint32_t) {
    return true;
}

Return<void> BiometricsFingerprint::onFingerDown(uint32_t, uint32_t, float, float) {
    setHbmState(HbmState::HBM_ON);
    extraApiWrapper(TouchCmd::CMD_FINGER_DOWN);

    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        this->onFingerUp();
    }).detach();

    return Void();
}

Return<void> BiometricsFingerprint::onFingerUp() {
    setHbmState(HbmState::HBM_OFF);
    extraApiWrapper(TouchCmd::CMD_FINGER_UP);
    return Void();
}

Return<void> BiometricsFingerprint::extraApiWrapper(int cidValue) {
    const int cmd = cidValue;

    hidl_vec<uint8_t> cmdData;
    cmdData.setToExternal(reinterpret_cast<uint8_t*>(const_cast<int*>(&cmd)), sizeof(cmd));

    mFingerprintRbs->extra_api(TouchCmd::PID_TOUCH, cmdData, [](const hidl_vec<uint8_t>&) {});

    return Void();
}

Return<void> BiometricsFingerprint::setHbmState(int state) {
    panel_param_info param_info = {.param_idx = PARAM_HBM, .value = state};

    int32_t node = open("/dev/dri/card0", O_RDWR);
    if (node < 0) {
        LOG(ERROR) << "Failed to open /dev/dri/card0";
        return Void();
    }

    int32_t ret = ioctl(node, DRM_IOCTL_SET_PANEL_FEATURE, &param_info);
    if (ret < 0) {
        LOG(ERROR) << "IOCTL failed with ret = " << ret;
    } else {
        LOG(INFO) << "HBM state set to " << state;
    }

    close(node);
    return Void();
}

}  // namespace implementation
}  // namespace V2_3
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace hardware
}  // namespace android
