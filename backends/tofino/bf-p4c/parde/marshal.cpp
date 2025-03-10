/**
 * Copyright (C) 2024 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations under the License.
 *
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "marshal.h"

#include "ir/ir.h"
#include "ir/json_generator.h"
#include "ir/json_loader.h"

std::string MarshaledFrom::toString() const {
    std::stringstream tmp;
    tmp << *this;
    return tmp.str();
}

void MarshaledFrom::toJSON(JSONGenerator &json) const {
    json.emit("gress", gress);
    json.emit("field_name", field_name);
    json.emit("pre_padding", pre_padding);
}

/* static */
MarshaledFrom MarshaledFrom::fromJSON(JSONLoader &json) {
    MarshaledFrom rv;
    json.load("gress", rv.gress);
    json.load("field_name", rv.field_name);
    json.load("pre_padding", rv.pre_padding);
    return rv;
}
