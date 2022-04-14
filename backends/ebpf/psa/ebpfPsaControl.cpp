/*
Copyright 2022-present Orange
Copyright 2022-present Open Networking Foundation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "ebpfPsaControl.h"

namespace EBPF {

ControlBodyTranslatorPSA::ControlBodyTranslatorPSA(const EBPFControlPSA* control) :
        CodeGenInspector(control->program->refMap, control->program->typeMap),
        ControlBodyTranslator(control) {}

bool ControlBodyTranslatorPSA::preorder(const IR::AssignmentStatement* a) {
    if (auto methodCallExpr = a->right->to<IR::MethodCallExpression>()) {
        auto mi = P4::MethodInstance::resolve(methodCallExpr,
                                              control->program->refMap,
                                              control->program->typeMap);
        auto ext = mi->to<P4::ExternMethod>();
        if (ext == nullptr) {
            return false;
        }

        if (ext->originalExternType->name.name == "Register" &&
            ext->method->type->name == "read") {
            cstring name = EBPFObject::externalName(ext->object);
            auto reg = control->to<EBPFControlPSA>()->getRegister(name);
            reg->emitRegisterRead(builder, ext, this, a->left);
            return false;
        }
    }

    return CodeGenInspector::preorder(a);
}

void ControlBodyTranslatorPSA::processMethod(const P4::ExternMethod* method) {
    auto decl = method->object;
    auto declType = method->originalExternType;
    cstring name = EBPFObject::externalName(decl);

    if (declType->name.name == "Counter") {
        auto counterMap = control->getCounter(name);
        counterMap->to<EBPFCounterPSA>()->emitMethodInvocation(builder, method, this);
        return;
    } else if (declType->name.name == "Register") {
        auto reg = control->to<EBPFControlPSA>()->getRegister(name);
        if (method->method->type->name == "write") {
            reg->emitRegisterWrite(builder, method, this);
        } else if (method->method->type->name == "read") {
            ::warning(ErrorType::WARN_UNUSED, "This Register(%1%) read value is not used!", name);
            reg->emitRegisterRead(builder, method, this, nullptr);
        }
        return;
    } else {
        ::error(ErrorType::ERR_UNSUPPORTED_ON_TARGET,
                "%1%: Unexpected method call", method->expr);
    }
}

cstring ControlBodyTranslatorPSA::getParamName(const IR::PathExpression *expr) {
    return expr->path->name.name;
}

void EBPFControlPSA::emitTableTypes(CodeBuilder *builder) {
    EBPFControl::emitTableTypes(builder);

    for (auto it : registers)
        it.second->emitTypes(builder);
}

void EBPFControlPSA::emitTableInstances(CodeBuilder* builder) {
    for (auto it : tables)
        it.second->emitInstance(builder);
    for (auto it : counters)
        it.second->emitInstance(builder);
    for (auto it : registers)
        it.second->emitInstance(builder);
}

void EBPFControlPSA::emitTableInitializers(CodeBuilder* builder) {
    for (auto it : tables) {
        it.second->emitInitializer(builder);
    }
    for (auto it : registers) {
        it.second->emitInitializer(builder);
    }
}

}