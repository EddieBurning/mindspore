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

#include "src/runtime/kernel/arm/int8/pad_int8.h"

using mindspore::lite::RET_ERROR;
using mindspore::lite::RET_MEMORY_FAILED;
using mindspore::lite::RET_OK;

namespace mindspore::kernel {
void PadInt8CPUKernel::FreeQuantParam() {
  if (pad_param_->pad_quant_arg_.in_quant_args_ != nullptr) {
    free(pad_param_->pad_quant_arg_.in_quant_args_);
    pad_param_->pad_quant_arg_.in_quant_args_ = nullptr;
  }
  if (pad_param_->pad_quant_arg_.out_quanr_args_ != nullptr) {
    free(pad_param_->pad_quant_arg_.out_quanr_args_);
    pad_param_->pad_quant_arg_.out_quanr_args_ = nullptr;
  }
}

int PadInt8CPUKernel::SetQuantParam() {
  PadQuantArg *pad_quant_args = &pad_param_->pad_quant_arg_;
  pad_quant_args->in_quant_args_ = reinterpret_cast<QuantArg *>(malloc(sizeof(QuantArg)));
  if (pad_quant_args->in_quant_args_ == nullptr) {
    return RET_MEMORY_FAILED;
  }
  pad_quant_args->out_quanr_args_ = reinterpret_cast<QuantArg *>(malloc(sizeof(QuantArg)));
  if (pad_quant_args->out_quanr_args_ == nullptr) {
    return RET_MEMORY_FAILED;
  }
  pad_quant_args->constant_value_ = reinterpret_cast<int8_t *>(malloc(sizeof(int8_t)));
  if (pad_quant_args->constant_value_ == nullptr) {
    return RET_MEMORY_FAILED;
  }

  auto *input_tensor = inputs_.at(kInputIndex);
  auto *out_tensor = outputs_.at(kOutputIndex);
  auto in_quant_arg = input_tensor->GetQuantParams();
  auto out_quant_arg = out_tensor->GetQuantParams();

  pad_quant_args->in_quant_args_->zp_ = in_quant_arg.front().zeroPoint;
  pad_quant_args->in_quant_args_->scale_ = in_quant_arg.front().scale;
  pad_quant_args->out_quanr_args_->zp_ = out_quant_arg.front().zeroPoint;
  pad_quant_args->out_quanr_args_->scale_ = out_quant_arg.front().scale;

  if (pad_quant_args->in_quant_args_->scale_ != pad_quant_args->out_quanr_args_->scale_ ||
      pad_quant_args->in_quant_args_->zp_ != pad_quant_args->out_quanr_args_->zp_) {
    MS_LOG(ERROR) << "Pad int8 op : scale & zp of output and input must be equal.";
    return RET_ERROR;
  }

  pad_quant_args->constant_value_[0] = QuantizeToInt8(
    pad_param_->constant_value_, pad_quant_args->in_quant_args_->scale_, pad_quant_args->in_quant_args_->zp_);
  return RET_OK;
}

int PadInt8CPUKernel::InitPadParam() {
  auto in_dims = inputs_[0]->shape();
  auto out_dims = outputs_[0]->shape();
  int ndims = in_dims.size();

  int in[] = {1, 1, 1, 1};
  int out[] = {1, 1, 1, 1};

  for (int i = 0; i < ndims; i++) {
    in[DEFAULT_PAD_NDIMS - ndims + i] = in_dims[i];
    out[DEFAULT_PAD_NDIMS - ndims + i] = out_dims[i];
  }

  memcpy(in_dims_, in, DEFAULT_PAD_NDIMS * sizeof(int));
  memcpy(out_dims_, out, DEFAULT_PAD_NDIMS * sizeof(int));

  return RET_OK;
}

int PadInt8CPUKernel::ReSize() {
  InitPadParam();
  return RET_OK;
}

int PadInt8CPUKernel::Init() {
  int error_code = InitPadParam();
  if (error_code != RET_OK) {
    MS_LOG(ERROR) << "InitPadParam failed. errorcode: " << error_code;
    return error_code;
  }

  error_code = SetQuantParam();
  if (error_code != RET_OK) {
    MS_LOG(ERROR) << "SetQuantParam failed. errorcode: " << error_code;
    return error_code;
  }
  return RET_OK;
}

int PadInt8CPUKernel::Run() {
  int8_t *in_data = reinterpret_cast<int8_t *>(inputs_[0]->Data());
  int8_t *out_data = reinterpret_cast<int8_t *>(outputs_[0]->Data());

  memset(out_data, pad_param_->pad_quant_arg_.constant_value_[0], outputs_[0]->ElementsNum() * sizeof(int8_t));
  PadConstant4D(in_data, out_data, in_dims_, out_dims_, pad_param_->paddings_);
  return RET_OK;
}

}  // namespace mindspore::kernel
