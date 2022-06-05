/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/vm.h"
#endif

#include "thirdparty/cJSON/cJSON.h" //<< AMALG_INLINE >>
#include "thirdparty/cJSON/cJSON.c" //<< AMALG_INLINE >>

// TODO:
// Convert a cJSON object into pocket variable. Note that the depth shouldn't
// exceed MAX_TEMP_REFERENCE of the PKVM and we should throw error if the dept
// is too much (cJSON complain at depth 1000), add depth as a parameter.
static Var _cJsonToPocket(PKVM* vm, cJSON* item) {

  switch ((item->type) & 0xFF) {
    case cJSON_NULL:
      return VAR_NULL;

    case cJSON_False:
      return VAR_FALSE;

    case cJSON_True:
      return VAR_TRUE;

    case cJSON_Number:
      return VAR_NUM(item->valuedouble);

    case cJSON_Raw:
    case cJSON_String: {
      const char* s = item->valuestring ? item->valuestring : "";
      return VAR_OBJ(newString(vm, s));
    }

    case cJSON_Array: {
      List* list = newList(vm, 8);
      vmPushTempRef(vm, &list->_super); // list.

      cJSON* elem = item->child;
      while (elem != NULL) {
        Var v = _cJsonToPocket(vm, elem);
        if (IS_OBJ(v)) vmPushTempRef(vm, AS_OBJ(v)); // v.
        listAppend(vm, list, v);
        if (IS_OBJ(v)) vmPopTempRef(vm); // v.
        elem = elem->next;
      }
      vmPopTempRef(vm); // list.
      return VAR_OBJ(list);
    }

    case cJSON_Object: {
      Map* map = newMap(vm);
      vmPushTempRef(vm, &map->_super); // map.
      cJSON* elem = item->child;
      while (elem != NULL) {
        String* key = newString(vm, elem->string);
        vmPushTempRef(vm, &key->_super); // key.
        {
          Var value = _cJsonToPocket(vm, elem);
          if (IS_OBJ(value)) vmPushTempRef(vm, AS_OBJ(value)); // value.
          mapSet(vm, map, VAR_OBJ(key), value);
          if (IS_OBJ(value)) vmPopTempRef(vm); // value.
        }
        vmPopTempRef(vm); // key.
        elem = elem->next;
      }
      vmPopTempRef(vm); // map.
      return VAR_OBJ(map);
    }

  default:
    UNREACHABLE();
  }

  UNREACHABLE();
}

static cJSON* _pocketToCJson(PKVM* vm, Var item) {
  PkVarType vt = getVarType(item);
  switch (vt) {
    case PK_NULL:
      return cJSON_CreateNull();

    case PK_BOOL:
      return cJSON_CreateBool(AS_BOOL(item));

    case PK_NUMBER:
      return cJSON_CreateNumber(AS_NUM(item));

    case PK_STRING:
      return cJSON_CreateString(((String*) AS_OBJ(item))->data);

    case PK_LIST: {
      List* list = (List*) AS_OBJ(item);
      cJSON* arr = cJSON_CreateArray();

      bool err = false;
      for (uint32_t i = 0; i < list->elements.count; i++) {
        cJSON* elem = _pocketToCJson(vm, list->elements.data[i]);
        if (elem == NULL) {
          err = true; break;
        };
        cJSON_AddItemToArray(arr, elem);
      }

      if (err) {
        cJSON_Delete(arr);
        return NULL;
      }

      return arr;
    }

    case PK_MAP: {
      Map* map = (Map*) AS_OBJ(item);
      cJSON* obj = cJSON_CreateObject();

      bool err = false;
      MapEntry* e = map->entries;
      for (; e < map->entries + map->capacity; e++) {
        if (IS_UNDEF(e->key)) continue;

        if (!IS_OBJ_TYPE(e->key, OBJ_STRING)) {
          pkSetRuntimeErrorFmt(vm, "Expected string as json object key, "
                               "instead got type '%s'.", varTypeName(e->key));
          err = true; break;
        }

        cJSON* value = _pocketToCJson(vm, e->value);
        if (value == NULL) { err = true; break; }

        cJSON_AddItemToObject(obj, ((String*) AS_OBJ(e->key))->data, value);
      }

      if (err) {
        cJSON_Delete(obj);
        return NULL;
      }

      return obj;
    }

    default: {
      pkSetRuntimeErrorFmt(vm, "Object of type '%s' cannot be serialized "
                          "to json.", varTypeName(item));
      return NULL;
    }

  }

  UNREACHABLE();
  return NULL;
}

DEF(_jsonParse,
  "json.parse(json_str:String) -> Var",
  "Parse a json string into pocket lang object.") {

  const char* string;
  if (!pkValidateSlotString(vm, 1, &string, NULL)) return;

  cJSON* tree = cJSON_Parse(string);

  if (tree == NULL) {

    // TODO: Print the position.
    // const char* position = cJSON_GetErrorPtr();
    // if (position != NULL) ...
    pkSetRuntimeError(vm, "Invalid json string");
    return;
  }

  Var obj = _cJsonToPocket(vm, tree);
  cJSON_Delete(tree);

  // Json is a standard libray "std_json" and has the direct access
  // to the vm's internal stack.
  vm->fiber->ret[0] = obj;
}

DEF(_jsonPrint,
  "json.print(value:Var, pretty:Bool=false)",
  "Render a pocketlang value into text. Takes an optional argument pretty, if "
  "true it'll pretty print the output.") {

  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 1, 2)) return;

  bool pretty = false;
  if (argc == 2) {
    if (!pkValidateSlotBool(vm, 2, &pretty)) return;
  }

  Var value = vm->fiber->ret[1];
  cJSON* json = _pocketToCJson(vm, value);

  // A runtime error already set.
  if (json == NULL) return;

  char* string = (pretty)
    ? cJSON_Print(json)
    : cJSON_PrintUnformatted(json);
  cJSON_Delete(json);

  if (string == NULL) {
    pkSetRuntimeError(vm, "Failed to print json.");
    return;
  }

  pkSetSlotString(vm, 0, string);
  free(string);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleJson(PKVM* vm) {
  PkHandle* json = pkNewModule(vm, "json");

  REGISTER_FN(json, "parse", _jsonParse, 1);
  REGISTER_FN(json, "print", _jsonPrint, -1);

  pkRegisterModule(vm, json);
  pkReleaseHandle(vm, json);
}
