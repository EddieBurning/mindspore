/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_BACKEND_SESSION_KERNEL_BUILD_CLIENT_H_
#define MINDSPORE_CCSRC_BACKEND_SESSION_KERNEL_BUILD_CLIENT_H_

#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <memory>

#include "common/duplex_pipe.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace kernel {
void ReplaceStr(std::string *dest, const std::string &replace, char new_char);
class KernelBuildClient {
 public:
  // Server configure
  constexpr inline static auto kEnv = "python";
  constexpr inline static auto kGetPathScript =
    "-c "
    "\""
    "import pkgutil;"
    "path = pkgutil"
    ".get_loader(\\\"mindspore._extends.remote.kernel_build_server\\\")"  // Server module name
    ".get_filename();"
    "print('[~]' + path)"
    "\"";

  // Receive the response from server
  constexpr inline static auto kAck = "ACK";
  constexpr inline static auto kErr = "ERR";
  constexpr inline static auto kFailed = "-1";
  // Send Finish request to server
  constexpr inline static auto kFin = "FIN";

  // Send building request to server
  constexpr inline static auto kTbeStart = "TBE/START";
  constexpr inline static auto kTbeWait = "TBE/WAIT";
  constexpr inline static auto kCont = "CONT";
  constexpr inline static auto kSuccess = "Success";
  constexpr inline static auto kTbeReset = "TBE/RESET";
  constexpr inline static auto kAkgStart = "AKG/START";
  constexpr inline static auto kAkgData = "AKG/DATA";
  constexpr inline static auto kAkgWait = "AKG/WAIT";

  // Send server info. query to server
  constexpr inline static auto kFormat = "FORMAT";
  constexpr inline static auto kSupport = "SUPPORT";
  constexpr inline static auto kTrue = "True";

  // Revert \n, \r, [space].
  constexpr inline static auto kLF = "[LF]";
  constexpr inline static auto kCR = "[CR]";
  constexpr inline static auto kSP = "[SP]";

  // The TAG as prefix of real command from remote.
  constexpr inline static auto kTag = "[~]";

  constexpr inline static int kBufferSize = 4096;
  constexpr inline static unsigned int kTimeOutSeconds = 350;

  static KernelBuildClient &Instance() {
    static KernelBuildClient instance;
    return instance;
  }
  std::string GetScriptPath() {
    std::string cmd = kEnv;
    (void)cmd.append(1, ' ').append(kGetPathScript);
    FILE *fpipe = popen(cmd.c_str(), "r");
    if (fpipe == nullptr) {
      MS_LOG(EXCEPTION) << "popen failed, " << strerror(errno) << "(" << errno << ")";
    }
    bool start = false;
    std::string result;
    char buf[kBufferSize];
    while (std::fgets(buf, sizeof(buf), fpipe) != nullptr) {
      if (std::strncmp(buf, kTag, std::strlen(kTag)) == 0) {
        start = true;
      }
      // Filter with 'kTAG' and '\n'
      if (start) {
        auto size = std::strlen(buf);
        bool line_end = buf[size - 1] == '\n';
        result.append(buf, line_end ? size - 1 : size);
        if (line_end) {
          break;
        }
      }
    }
    pclose(fpipe);
    const std::string py_suffix = ".py";
    if (result.empty() || result.rfind(py_suffix) != (result.length() - py_suffix.length())) {
      MS_LOG(EXCEPTION) << "py file seems incorrect, result: {" << result << "}";
    }
    result = result.substr(strlen(kTag));
    MS_LOG(DEBUG) << "result: " << result;
    return result;
  }

  void Open() {
    if (!init_) {
      // Exception's thrown if open failed
      if (dp_->Open({kEnv, GetScriptPath()}, true) != -1) {
        dp_->SetTimeOutSeconds(kTimeOutSeconds);
        dp_->SetTimeOutCallback([this]() { SendRequest(kFin); });
        init_ = true;
      }
    }
  }
  void Close() {
    if (init_) {
      dp_->Close();
      init_ = false;
    }
  }

  // Send a request and fetch its response
  std::string SendRequest(std::string data) {
    Request(data);
    return Response();
  }
  void Request(std::string req) {
    if (!init_) {
      MS_LOG(EXCEPTION) << "Try to send request before Open()";
    }
    MS_LOG(DEBUG) << "\t[" << req << "]";
    *dp_ << req;
  }
  std::string Response() {
    if (!init_) {
      MS_LOG(EXCEPTION) << "Try to get response before Open()";
    }
    std::string res;
    *dp_ >> res;
    // Filter out the interference
    auto start = res.find(kTag);
    if (start == std::string::npos) {
      MS_LOG(EXCEPTION) << "Response seems incorrect, res: " << res;
    }
    res = res.substr(start + std::strlen(kTag), res.size() - start);
    // Revert the line feed and space
    if (res != kSuccess && res != kAck && res != kErr && res != kTrue) {
      ReplaceStr(&res, kLF, '\n');
      ReplaceStr(&res, kSP, ' ');
    }
    MS_LOG(DEBUG) << "\t[" << res << "]";
    return res;
  }

  // Before building.
  std::string SelectFormat(const std::string &json);
  bool CheckSupported(const std::string &json);

  // Run TBE building.
  int TbeStart(const std::string &json);
  bool TbeWait(int *task_id, std::string *task_result, std::string *pre_build_result);
  void TbeReset();

  // Run AKG building.
  bool AkgStart(int process_num, int wait_time);
  bool AkgSendData(const std::vector<std::string> &jsons);
  bool AkgWait();

  KernelBuildClient(const KernelBuildClient &) = delete;
  KernelBuildClient &operator=(const KernelBuildClient &) = delete;

  KernelBuildClient(KernelBuildClient &&) = delete;
  KernelBuildClient &operator=(KernelBuildClient &&) = delete;

 private:
  KernelBuildClient() : init_(false), dp_(std::make_shared<DuplexPipe>()) { Open(); }
  ~KernelBuildClient() { Close(); }

  bool init_;
  std::shared_ptr<DuplexPipe> dp_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_SESSION_KERNEL_BUILD_CLIENT_H_
