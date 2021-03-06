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

#ifndef MS_TFLITE_UTIL_H
#define MS_TFLITE_UTIL_H


#include <string>
#include "utils/log_adapter.h"
#include "schema/inner/model_generated.h"
#include "tools/converter/parser/tflite/schema_generated.h"
#include "schema/inner/ops_generated.h"
#include "ir/dtype/type_id.h"

// using namespace std;

namespace mindspore {
namespace lite {
schema::PadMode GetPadMode(tflite::Padding tflite_padmode);

size_t GetDataTypeSize(const TypeId &data_type);

schema::ActivationType GetActivationFunctionType(tflite::ActivationFunctionType tfliteAFType);

std::string GetMSOpType(tflite::BuiltinOperator tfliteOpType);

TypeId GetTfliteDataType(const tflite::TensorType &tflite_data_type);
}  // namespace lite
}  // namespace mindspore

#endif  // MS_TFLITE_UTIL_H

