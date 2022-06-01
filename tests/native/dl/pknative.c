/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <pocketlang.h>

// !! THIS FILE IS GENERATED DO NOT EDIT !!

typedef PkConfiguration (*pkNewConfiguration_t)();
typedef PKVM* (*pkNewVM_t)(PkConfiguration*);
typedef void (*pkFreeVM_t)(PKVM*);
typedef void (*pkSetUserData_t)(PKVM*, void*);
typedef void* (*pkGetUserData_t)(const PKVM*);
typedef void (*pkRegisterBuiltinFn_t)(PKVM*, const char*, pkNativeFn, int, const char*);
typedef void (*pkAddSearchPath_t)(PKVM*, const char*);
typedef void* (*pkRealloc_t)(PKVM*, void*, size_t);
typedef void (*pkReleaseHandle_t)(PKVM*, PkHandle*);
typedef PkHandle* (*pkNewModule_t)(PKVM*, const char*);
typedef void (*pkRegisterModule_t)(PKVM*, PkHandle*);
typedef void (*pkModuleAddFunction_t)(PKVM*, PkHandle*, const char*, pkNativeFn, int);
typedef PkHandle* (*pkNewClass_t)(PKVM*, const char*, PkHandle*, PkHandle*, pkNewInstanceFn, pkDeleteInstanceFn);
typedef void (*pkClassAddMethod_t)(PKVM*, PkHandle*, const char*, pkNativeFn, int);
typedef void (*pkModuleAddSource_t)(PKVM*, PkHandle*, const char*);
typedef PkResult (*pkRunString_t)(PKVM*, const char*);
typedef PkResult (*pkRunFile_t)(PKVM*, const char*);
typedef PkResult (*pkRunREPL_t)(PKVM*);
typedef void (*pkSetRuntimeError_t)(PKVM*, const char*);
typedef void* (*pkGetSelf_t)(const PKVM*);
typedef int (*pkGetArgc_t)(const PKVM*);
typedef bool (*pkCheckArgcRange_t)(PKVM*, int, int, int);
typedef bool (*pkValidateSlotBool_t)(PKVM*, int, bool*);
typedef bool (*pkValidateSlotNumber_t)(PKVM*, int, double*);
typedef bool (*pkValidateSlotInteger_t)(PKVM*, int, int32_t*);
typedef bool (*pkValidateSlotString_t)(PKVM*, int, const char**, uint32_t*);
typedef bool (*pkValidateSlotType_t)(PKVM*, int, PkVarType);
typedef bool (*pkValidateSlotInstanceOf_t)(PKVM*, int, int);
typedef bool (*pkIsSlotInstanceOf_t)(PKVM*, int, int, bool*);
typedef void (*pkReserveSlots_t)(PKVM*, int);
typedef int (*pkGetSlotsCount_t)(PKVM*);
typedef PkVarType (*pkGetSlotType_t)(PKVM*, int);
typedef bool (*pkGetSlotBool_t)(PKVM*, int);
typedef double (*pkGetSlotNumber_t)(PKVM*, int);
typedef const char* (*pkGetSlotString_t)(PKVM*, int, uint32_t*);
typedef PkHandle* (*pkGetSlotHandle_t)(PKVM*, int);
typedef void* (*pkGetSlotNativeInstance_t)(PKVM*, int);
typedef void (*pkSetSlotNull_t)(PKVM*, int);
typedef void (*pkSetSlotBool_t)(PKVM*, int, bool);
typedef void (*pkSetSlotNumber_t)(PKVM*, int, double);
typedef void (*pkSetSlotString_t)(PKVM*, int, const char*);
typedef void (*pkSetSlotStringLength_t)(PKVM*, int, const char*, uint32_t);
typedef void (*pkSetSlotHandle_t)(PKVM*, int, PkHandle*);
typedef uint32_t (*pkGetSlotHash_t)(PKVM*, int);
typedef void (*pkPlaceSelf_t)(PKVM*, int);
typedef void (*pkGetClass_t)(PKVM*, int, int);
typedef bool (*pkNewInstance_t)(PKVM*, int, int, int, int);
typedef void (*pkNewRange_t)(PKVM*, int, double, double);
typedef void (*pkNewList_t)(PKVM*, int);
typedef void (*pkNewMap_t)(PKVM*, int);
typedef bool (*pkListInsert_t)(PKVM*, int, int32_t, int);
typedef bool (*pkListPop_t)(PKVM*, int, int32_t, int);
typedef uint32_t (*pkListLength_t)(PKVM*, int);
typedef bool (*pkCallFunction_t)(PKVM*, int, int, int, int);
typedef bool (*pkCallMethod_t)(PKVM*, int, const char*, int, int, int);
typedef bool (*pkGetAttribute_t)(PKVM*, int, const char*, int);
typedef bool (*pkSetAttribute_t)(PKVM*, int, const char*, int);
typedef bool (*pkImportModule_t)(PKVM*, const char*, int);

typedef struct {
  pkNewConfiguration_t pkNewConfiguration_ptr;
  pkNewVM_t pkNewVM_ptr;
  pkFreeVM_t pkFreeVM_ptr;
  pkSetUserData_t pkSetUserData_ptr;
  pkGetUserData_t pkGetUserData_ptr;
  pkRegisterBuiltinFn_t pkRegisterBuiltinFn_ptr;
  pkAddSearchPath_t pkAddSearchPath_ptr;
  pkRealloc_t pkRealloc_ptr;
  pkReleaseHandle_t pkReleaseHandle_ptr;
  pkNewModule_t pkNewModule_ptr;
  pkRegisterModule_t pkRegisterModule_ptr;
  pkModuleAddFunction_t pkModuleAddFunction_ptr;
  pkNewClass_t pkNewClass_ptr;
  pkClassAddMethod_t pkClassAddMethod_ptr;
  pkModuleAddSource_t pkModuleAddSource_ptr;
  pkRunString_t pkRunString_ptr;
  pkRunFile_t pkRunFile_ptr;
  pkRunREPL_t pkRunREPL_ptr;
  pkSetRuntimeError_t pkSetRuntimeError_ptr;
  pkGetSelf_t pkGetSelf_ptr;
  pkGetArgc_t pkGetArgc_ptr;
  pkCheckArgcRange_t pkCheckArgcRange_ptr;
  pkValidateSlotBool_t pkValidateSlotBool_ptr;
  pkValidateSlotNumber_t pkValidateSlotNumber_ptr;
  pkValidateSlotInteger_t pkValidateSlotInteger_ptr;
  pkValidateSlotString_t pkValidateSlotString_ptr;
  pkValidateSlotType_t pkValidateSlotType_ptr;
  pkValidateSlotInstanceOf_t pkValidateSlotInstanceOf_ptr;
  pkIsSlotInstanceOf_t pkIsSlotInstanceOf_ptr;
  pkReserveSlots_t pkReserveSlots_ptr;
  pkGetSlotsCount_t pkGetSlotsCount_ptr;
  pkGetSlotType_t pkGetSlotType_ptr;
  pkGetSlotBool_t pkGetSlotBool_ptr;
  pkGetSlotNumber_t pkGetSlotNumber_ptr;
  pkGetSlotString_t pkGetSlotString_ptr;
  pkGetSlotHandle_t pkGetSlotHandle_ptr;
  pkGetSlotNativeInstance_t pkGetSlotNativeInstance_ptr;
  pkSetSlotNull_t pkSetSlotNull_ptr;
  pkSetSlotBool_t pkSetSlotBool_ptr;
  pkSetSlotNumber_t pkSetSlotNumber_ptr;
  pkSetSlotString_t pkSetSlotString_ptr;
  pkSetSlotStringLength_t pkSetSlotStringLength_ptr;
  pkSetSlotHandle_t pkSetSlotHandle_ptr;
  pkGetSlotHash_t pkGetSlotHash_ptr;
  pkPlaceSelf_t pkPlaceSelf_ptr;
  pkGetClass_t pkGetClass_ptr;
  pkNewInstance_t pkNewInstance_ptr;
  pkNewRange_t pkNewRange_ptr;
  pkNewList_t pkNewList_ptr;
  pkNewMap_t pkNewMap_ptr;
  pkListInsert_t pkListInsert_ptr;
  pkListPop_t pkListPop_ptr;
  pkListLength_t pkListLength_ptr;
  pkCallFunction_t pkCallFunction_ptr;
  pkCallMethod_t pkCallMethod_ptr;
  pkGetAttribute_t pkGetAttribute_ptr;
  pkSetAttribute_t pkSetAttribute_ptr;
  pkImportModule_t pkImportModule_ptr;
} PkNativeApi;

static PkNativeApi pk_api;

PK_EXPORT void pkInitApi(PkNativeApi* api) {
  pk_api.pkNewConfiguration_ptr = api->pkNewConfiguration_ptr;
  pk_api.pkNewVM_ptr = api->pkNewVM_ptr;
  pk_api.pkFreeVM_ptr = api->pkFreeVM_ptr;
  pk_api.pkSetUserData_ptr = api->pkSetUserData_ptr;
  pk_api.pkGetUserData_ptr = api->pkGetUserData_ptr;
  pk_api.pkRegisterBuiltinFn_ptr = api->pkRegisterBuiltinFn_ptr;
  pk_api.pkAddSearchPath_ptr = api->pkAddSearchPath_ptr;
  pk_api.pkRealloc_ptr = api->pkRealloc_ptr;
  pk_api.pkReleaseHandle_ptr = api->pkReleaseHandle_ptr;
  pk_api.pkNewModule_ptr = api->pkNewModule_ptr;
  pk_api.pkRegisterModule_ptr = api->pkRegisterModule_ptr;
  pk_api.pkModuleAddFunction_ptr = api->pkModuleAddFunction_ptr;
  pk_api.pkNewClass_ptr = api->pkNewClass_ptr;
  pk_api.pkClassAddMethod_ptr = api->pkClassAddMethod_ptr;
  pk_api.pkModuleAddSource_ptr = api->pkModuleAddSource_ptr;
  pk_api.pkRunString_ptr = api->pkRunString_ptr;
  pk_api.pkRunFile_ptr = api->pkRunFile_ptr;
  pk_api.pkRunREPL_ptr = api->pkRunREPL_ptr;
  pk_api.pkSetRuntimeError_ptr = api->pkSetRuntimeError_ptr;
  pk_api.pkGetSelf_ptr = api->pkGetSelf_ptr;
  pk_api.pkGetArgc_ptr = api->pkGetArgc_ptr;
  pk_api.pkCheckArgcRange_ptr = api->pkCheckArgcRange_ptr;
  pk_api.pkValidateSlotBool_ptr = api->pkValidateSlotBool_ptr;
  pk_api.pkValidateSlotNumber_ptr = api->pkValidateSlotNumber_ptr;
  pk_api.pkValidateSlotInteger_ptr = api->pkValidateSlotInteger_ptr;
  pk_api.pkValidateSlotString_ptr = api->pkValidateSlotString_ptr;
  pk_api.pkValidateSlotType_ptr = api->pkValidateSlotType_ptr;
  pk_api.pkValidateSlotInstanceOf_ptr = api->pkValidateSlotInstanceOf_ptr;
  pk_api.pkIsSlotInstanceOf_ptr = api->pkIsSlotInstanceOf_ptr;
  pk_api.pkReserveSlots_ptr = api->pkReserveSlots_ptr;
  pk_api.pkGetSlotsCount_ptr = api->pkGetSlotsCount_ptr;
  pk_api.pkGetSlotType_ptr = api->pkGetSlotType_ptr;
  pk_api.pkGetSlotBool_ptr = api->pkGetSlotBool_ptr;
  pk_api.pkGetSlotNumber_ptr = api->pkGetSlotNumber_ptr;
  pk_api.pkGetSlotString_ptr = api->pkGetSlotString_ptr;
  pk_api.pkGetSlotHandle_ptr = api->pkGetSlotHandle_ptr;
  pk_api.pkGetSlotNativeInstance_ptr = api->pkGetSlotNativeInstance_ptr;
  pk_api.pkSetSlotNull_ptr = api->pkSetSlotNull_ptr;
  pk_api.pkSetSlotBool_ptr = api->pkSetSlotBool_ptr;
  pk_api.pkSetSlotNumber_ptr = api->pkSetSlotNumber_ptr;
  pk_api.pkSetSlotString_ptr = api->pkSetSlotString_ptr;
  pk_api.pkSetSlotStringLength_ptr = api->pkSetSlotStringLength_ptr;
  pk_api.pkSetSlotHandle_ptr = api->pkSetSlotHandle_ptr;
  pk_api.pkGetSlotHash_ptr = api->pkGetSlotHash_ptr;
  pk_api.pkPlaceSelf_ptr = api->pkPlaceSelf_ptr;
  pk_api.pkGetClass_ptr = api->pkGetClass_ptr;
  pk_api.pkNewInstance_ptr = api->pkNewInstance_ptr;
  pk_api.pkNewRange_ptr = api->pkNewRange_ptr;
  pk_api.pkNewList_ptr = api->pkNewList_ptr;
  pk_api.pkNewMap_ptr = api->pkNewMap_ptr;
  pk_api.pkListInsert_ptr = api->pkListInsert_ptr;
  pk_api.pkListPop_ptr = api->pkListPop_ptr;
  pk_api.pkListLength_ptr = api->pkListLength_ptr;
  pk_api.pkCallFunction_ptr = api->pkCallFunction_ptr;
  pk_api.pkCallMethod_ptr = api->pkCallMethod_ptr;
  pk_api.pkGetAttribute_ptr = api->pkGetAttribute_ptr;
  pk_api.pkSetAttribute_ptr = api->pkSetAttribute_ptr;
  pk_api.pkImportModule_ptr = api->pkImportModule_ptr;
}

PkConfiguration pkNewConfiguration() {
  return pk_api.pkNewConfiguration_ptr();
}

PKVM* pkNewVM(PkConfiguration* config) {
  return pk_api.pkNewVM_ptr(config);
}

void pkFreeVM(PKVM* vm) {
  pk_api.pkFreeVM_ptr(vm);
}

void pkSetUserData(PKVM* vm, void* user_data) {
  pk_api.pkSetUserData_ptr(vm, user_data);
}

void* pkGetUserData(const PKVM* vm) {
  return pk_api.pkGetUserData_ptr(vm);
}

void pkRegisterBuiltinFn(PKVM* vm, const char* name, pkNativeFn fn, int arity, const char* docstring) {
  pk_api.pkRegisterBuiltinFn_ptr(vm, name, fn, arity, docstring);
}

void pkAddSearchPath(PKVM* vm, const char* path) {
  pk_api.pkAddSearchPath_ptr(vm, path);
}

void* pkRealloc(PKVM* vm, void* ptr, size_t size) {
  return pk_api.pkRealloc_ptr(vm, ptr, size);
}

void pkReleaseHandle(PKVM* vm, PkHandle* handle) {
  pk_api.pkReleaseHandle_ptr(vm, handle);
}

PkHandle* pkNewModule(PKVM* vm, const char* name) {
  return pk_api.pkNewModule_ptr(vm, name);
}

void pkRegisterModule(PKVM* vm, PkHandle* module) {
  pk_api.pkRegisterModule_ptr(vm, module);
}

void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name, pkNativeFn fptr, int arity) {
  pk_api.pkModuleAddFunction_ptr(vm, module, name, fptr, arity);
}

PkHandle* pkNewClass(PKVM* vm, const char* name, PkHandle* base_class, PkHandle* module, pkNewInstanceFn new_fn, pkDeleteInstanceFn delete_fn) {
  return pk_api.pkNewClass_ptr(vm, name, base_class, module, new_fn, delete_fn);
}

void pkClassAddMethod(PKVM* vm, PkHandle* cls, const char* name, pkNativeFn fptr, int arity) {
  pk_api.pkClassAddMethod_ptr(vm, cls, name, fptr, arity);
}

void pkModuleAddSource(PKVM* vm, PkHandle* module, const char* source) {
  pk_api.pkModuleAddSource_ptr(vm, module, source);
}

PkResult pkRunString(PKVM* vm, const char* source) {
  return pk_api.pkRunString_ptr(vm, source);
}

PkResult pkRunFile(PKVM* vm, const char* path) {
  return pk_api.pkRunFile_ptr(vm, path);
}

PkResult pkRunREPL(PKVM* vm) {
  return pk_api.pkRunREPL_ptr(vm);
}

void pkSetRuntimeError(PKVM* vm, const char* message) {
  pk_api.pkSetRuntimeError_ptr(vm, message);
}

void* pkGetSelf(const PKVM* vm) {
  return pk_api.pkGetSelf_ptr(vm);
}

int pkGetArgc(const PKVM* vm) {
  return pk_api.pkGetArgc_ptr(vm);
}

bool pkCheckArgcRange(PKVM* vm, int argc, int min, int max) {
  return pk_api.pkCheckArgcRange_ptr(vm, argc, min, max);
}

bool pkValidateSlotBool(PKVM* vm, int slot, bool* value) {
  return pk_api.pkValidateSlotBool_ptr(vm, slot, value);
}

bool pkValidateSlotNumber(PKVM* vm, int slot, double* value) {
  return pk_api.pkValidateSlotNumber_ptr(vm, slot, value);
}

bool pkValidateSlotInteger(PKVM* vm, int slot, int32_t* value) {
  return pk_api.pkValidateSlotInteger_ptr(vm, slot, value);
}

bool pkValidateSlotString(PKVM* vm, int slot, const char** value, uint32_t* length) {
  return pk_api.pkValidateSlotString_ptr(vm, slot, value, length);
}

bool pkValidateSlotType(PKVM* vm, int slot, PkVarType type) {
  return pk_api.pkValidateSlotType_ptr(vm, slot, type);
}

bool pkValidateSlotInstanceOf(PKVM* vm, int slot, int cls) {
  return pk_api.pkValidateSlotInstanceOf_ptr(vm, slot, cls);
}

bool pkIsSlotInstanceOf(PKVM* vm, int inst, int cls, bool* val) {
  return pk_api.pkIsSlotInstanceOf_ptr(vm, inst, cls, val);
}

void pkReserveSlots(PKVM* vm, int count) {
  pk_api.pkReserveSlots_ptr(vm, count);
}

int pkGetSlotsCount(PKVM* vm) {
  return pk_api.pkGetSlotsCount_ptr(vm);
}

PkVarType pkGetSlotType(PKVM* vm, int index) {
  return pk_api.pkGetSlotType_ptr(vm, index);
}

bool pkGetSlotBool(PKVM* vm, int index) {
  return pk_api.pkGetSlotBool_ptr(vm, index);
}

double pkGetSlotNumber(PKVM* vm, int index) {
  return pk_api.pkGetSlotNumber_ptr(vm, index);
}

const char* pkGetSlotString(PKVM* vm, int index, uint32_t* length) {
  return pk_api.pkGetSlotString_ptr(vm, index, length);
}

PkHandle* pkGetSlotHandle(PKVM* vm, int index) {
  return pk_api.pkGetSlotHandle_ptr(vm, index);
}

void* pkGetSlotNativeInstance(PKVM* vm, int index) {
  return pk_api.pkGetSlotNativeInstance_ptr(vm, index);
}

void pkSetSlotNull(PKVM* vm, int index) {
  pk_api.pkSetSlotNull_ptr(vm, index);
}

void pkSetSlotBool(PKVM* vm, int index, bool value) {
  pk_api.pkSetSlotBool_ptr(vm, index, value);
}

void pkSetSlotNumber(PKVM* vm, int index, double value) {
  pk_api.pkSetSlotNumber_ptr(vm, index, value);
}

void pkSetSlotString(PKVM* vm, int index, const char* value) {
  pk_api.pkSetSlotString_ptr(vm, index, value);
}

void pkSetSlotStringLength(PKVM* vm, int index, const char* value, uint32_t length) {
  pk_api.pkSetSlotStringLength_ptr(vm, index, value, length);
}

void pkSetSlotHandle(PKVM* vm, int index, PkHandle* handle) {
  pk_api.pkSetSlotHandle_ptr(vm, index, handle);
}

uint32_t pkGetSlotHash(PKVM* vm, int index) {
  return pk_api.pkGetSlotHash_ptr(vm, index);
}

void pkPlaceSelf(PKVM* vm, int index) {
  pk_api.pkPlaceSelf_ptr(vm, index);
}

void pkGetClass(PKVM* vm, int instance, int index) {
  pk_api.pkGetClass_ptr(vm, instance, index);
}

bool pkNewInstance(PKVM* vm, int cls, int index, int argc, int argv) {
  return pk_api.pkNewInstance_ptr(vm, cls, index, argc, argv);
}

void pkNewRange(PKVM* vm, int index, double first, double last) {
  pk_api.pkNewRange_ptr(vm, index, first, last);
}

void pkNewList(PKVM* vm, int index) {
  pk_api.pkNewList_ptr(vm, index);
}

void pkNewMap(PKVM* vm, int index) {
  pk_api.pkNewMap_ptr(vm, index);
}

bool pkListInsert(PKVM* vm, int list, int32_t index, int value) {
  return pk_api.pkListInsert_ptr(vm, list, index, value);
}

bool pkListPop(PKVM* vm, int list, int32_t index, int popped) {
  return pk_api.pkListPop_ptr(vm, list, index, popped);
}

uint32_t pkListLength(PKVM* vm, int list) {
  return pk_api.pkListLength_ptr(vm, list);
}

bool pkCallFunction(PKVM* vm, int fn, int argc, int argv, int ret) {
  return pk_api.pkCallFunction_ptr(vm, fn, argc, argv, ret);
}

bool pkCallMethod(PKVM* vm, int instance, const char* method, int argc, int argv, int ret) {
  return pk_api.pkCallMethod_ptr(vm, instance, method, argc, argv, ret);
}

bool pkGetAttribute(PKVM* vm, int instance, const char* name, int index) {
  return pk_api.pkGetAttribute_ptr(vm, instance, name, index);
}

bool pkSetAttribute(PKVM* vm, int instance, const char* name, int value) {
  return pk_api.pkSetAttribute_ptr(vm, instance, name, value);
}

bool pkImportModule(PKVM* vm, const char* path, int index) {
  return pk_api.pkImportModule_ptr(vm, path, index);
}
