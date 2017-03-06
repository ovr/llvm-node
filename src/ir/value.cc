//
// Created by Micha Reiser on 01.03.17.
//

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include "../util/string.h"
#include "value.h"
#include "basic-block.h"
#include "constants.h"
#include "type.h"
#include "phi-node.h"
#include "alloca-inst.h"

NAN_MODULE_INIT(ValueWrapper::Init) {
    auto object = Nan::New<v8::Object>();

    Nan::Set(object, Nan::New("MaxAlignmentExponent").ToLocalChecked(), Nan::New(llvm::Value::MaxAlignmentExponent));
    Nan::Set(object, Nan::New("MaximumAlignment").ToLocalChecked(), Nan::New(llvm::Value::MaximumAlignment));

    Nan::Set(target, Nan::New("Value").ToLocalChecked(), object);
}

Nan::Persistent<v8::FunctionTemplate>& ValueWrapper::valueTemplate() {
    static Nan::Persistent<v8::FunctionTemplate> functionTemplate{};

    if (functionTemplate.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> localTemplate = Nan::New<v8::FunctionTemplate>(ValueWrapper::New);
        localTemplate->SetClassName(Nan::New("Value").ToLocalChecked());
        localTemplate->InstanceTemplate()->SetInternalFieldCount(1);

        Nan::SetPrototypeMethod(localTemplate, "dump", ValueWrapper::dump);
        Nan::SetPrototypeMethod(localTemplate, "hasName", ValueWrapper::hasName);
        Nan::SetAccessor(localTemplate->InstanceTemplate(), Nan::New("type").ToLocalChecked(), ValueWrapper::getType);
        Nan::SetAccessor(localTemplate->InstanceTemplate(), Nan::New("name").ToLocalChecked(), ValueWrapper::getName,
                         ValueWrapper::setName);
        Nan::SetPrototypeMethod(localTemplate, "release", BasicBlockWrapper::release);
        Nan::SetPrototypeMethod(localTemplate, "replaceAllUsesWith", BasicBlockWrapper::replaceAllUsesWith);
        Nan::SetPrototypeMethod(localTemplate, "useEmpty", BasicBlockWrapper::useEmpty);

        functionTemplate.Reset(localTemplate);
    }

    return functionTemplate;
}

NAN_METHOD(ValueWrapper::New) {
    if (!info.IsConstructCall()) {
        return Nan::ThrowTypeError("Class Constructor Value cannot be invoked without new");
    }

    if (info.Length() != 1 || !info[0]->IsExternal()) {
        return Nan::ThrowTypeError("External Value Pointer required");
    }

    llvm::Value* value = static_cast<llvm::Value*>(v8::External::Cast(*info[0])->Value());
    auto* wrapper = new ValueWrapper { value };
    wrapper->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
}

NAN_METHOD(ValueWrapper::dump) {
    ValueWrapper::FromValue(info.Holder())->value->dump();
}

NAN_GETTER(ValueWrapper::getType) {
    auto* type = ValueWrapper::FromValue(info.Holder())->value->getType();

    info.GetReturnValue().Set(TypeWrapper::of(type));
}

NAN_METHOD(ValueWrapper::hasName) {
    auto* wrapper = ValueWrapper::FromValue(info.Holder());

    info.GetReturnValue().Set(Nan::New(wrapper->value->hasName()));
}

NAN_GETTER(ValueWrapper::getName) {
    auto* wrapper = ValueWrapper::FromValue(info.Holder());

    v8::Local<v8::String> name = Nan::New(wrapper->value->getName().str()).ToLocalChecked();
    info.GetReturnValue().Set(name);
}

NAN_SETTER(ValueWrapper::setName) {
    if (!value->IsString()) {
        return Nan::ThrowTypeError("String required");
    }

    auto name = Nan::To<v8::String>(value).ToLocalChecked();
    ValueWrapper::FromValue(info.Holder())->value->setName(ToString(name));
}

NAN_METHOD(ValueWrapper::release) {
    auto* wrapper = ValueWrapper::FromValue(info.Holder());
    delete wrapper->getValue();
}

NAN_METHOD(ValueWrapper::replaceAllUsesWith) {
    if (info.Length() != 1 || !ValueWrapper::isInstance(info[0])) {
        return Nan::ThrowTypeError("replaceAllUsesWith needs to be called with: value: Value");
    }

    auto* self = ValueWrapper::FromValue(info.Holder())->getValue();
    auto* value = ValueWrapper::FromValue(info[0])->getValue();

    self->replaceAllUsesWith(value);
}

NAN_METHOD(ValueWrapper::useEmpty) {
    auto* value = ValueWrapper::FromValue(info.Holder())->getValue();
    info.GetReturnValue().Set(Nan::New(value->use_empty()));
}

v8::Local<v8::Object> ValueWrapper::of(llvm::Value *value) {
    v8::Local<v8::Object> result {};

    if (llvm::Constant::classof(value)) {
        result = ConstantWrapper::of(static_cast<llvm::Constant*>(value));
    } else if (llvm::BasicBlock::classof(value)) {
        result = BasicBlockWrapper::of(static_cast<llvm::BasicBlock*>(value));
    } else if(llvm::PHINode::classof(value)) {
        result = PhiNodeWrapper::of(static_cast<llvm::PHINode*>(value));
    } else if(llvm::AllocaInst::classof(value)) {
        result = AllocaInstWrapper::of(static_cast<llvm::AllocaInst*>(value));
    } else {
        auto constructorFunction = Nan::GetFunction(Nan::New(valueTemplate())).ToLocalChecked();
        v8::Local<v8::Value> args[1] = { Nan::New<v8::External>(value) };
        result = Nan::NewInstance(constructorFunction, 1, args).ToLocalChecked();
    }

    Nan::EscapableHandleScope escapeScope {};
    return escapeScope.Escape(result);
}

bool ValueWrapper::isInstance(v8::Local<v8::Value> value) {
    return Nan::New(valueTemplate())->HasInstance(value);
}

llvm::Value *ValueWrapper::getValue() {
    return value;
}


