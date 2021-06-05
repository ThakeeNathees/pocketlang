/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "var.h"

#include <stdio.h>
#include "utils.h"
#include "vm.h"

/*****************************************************************************/
/* PUBLIC API                                                                */
/*****************************************************************************/

PkVarType pkGetValueType(const PkVar value) {
  __ASSERT(value != NULL, "Given value was NULL.");

  if (IS_NULL(*(const Var*)(value))) return PK_NULL;
  if (IS_BOOL(*(const Var*)(value))) return PK_BOOL;
  if (IS_NUM(*(const Var*)(value))) return PK_NUMBER;

  __ASSERT(IS_OBJ(*(const Var*)(value)),
           "Invalid var pointer. Might be a dangling pointer");

  const Object* obj = AS_OBJ(*(const Var*)(value));
  switch (obj->type) {
    case OBJ_STRING: return PK_STRING;
    case OBJ_LIST:   return PK_LIST;
    case OBJ_MAP:    return PK_MAP;
    case OBJ_RANGE:  return PK_RANGE;
    case OBJ_SCRIPT: return PK_SCRIPT;
    case OBJ_FUNC:   return PK_FUNCTION;
    case OBJ_FIBER:  return PK_FIBER;
    case OBJ_USER:   TODO; break;
  }

  UNREACHABLE();
  return (PkVarType)0;
}

PkHandle* pkNewString(PKVM* vm, const char* value) {
  return vmNewHandle(vm, VAR_OBJ(newString(vm, value)));
}

PkHandle* pkNewStringLength(PKVM* vm, const char* value, size_t len) {
  return vmNewHandle(vm, VAR_OBJ(newStringLength(vm, value, (uint32_t)len)));
}

PkHandle* pkNewList(PKVM* vm) {
  return vmNewHandle(vm, VAR_OBJ(newList(vm, MIN_CAPACITY)));
}

PkHandle* pkNewMap(PKVM* vm) {
  return vmNewHandle(vm, VAR_OBJ(newMap(vm)));
}

const char* pkStringGetData(const PkVar value) {
  const Var str = (*(const Var*)value);
  __ASSERT(IS_OBJ_TYPE(str, OBJ_STRING), "Value should be of type string.");
  return ((String*)AS_OBJ(str))->data;
}

/*****************************************************************************/
/* VAR INTERNALS                                                             */
/*****************************************************************************/

// Number of maximum digits for to_string buffer.
#define TO_STRING_BUFF_SIZE 128

// The maximum percentage of the map entries that can be filled before the map
// is grown. A lower percentage reduce collision which makes looks up faster
// but take more memory.
#define MAP_LOAD_PERCENT 75

// The factor a collection would grow by when it's exceeds the current
// capacity. The new capacity will be calculated by multiplying it's old
// capacity by the GROW_FACTOR.
#define GROW_FACTOR 2

void varInitObject(Object* self, PKVM* vm, ObjectType type) {
  self->type = type;
  self->is_marked = false;
  self->next = vm->first;
  vm->first = self;
}

void grayObject(PKVM* vm, Object* self) {
  if (self == NULL || self->is_marked) return;
  self->is_marked = true;

  // Add the object to the VM's gray_list so that we can recursively mark
  // it's referenced objects later.
  if (vm->gray_list_count >= vm->gray_list_capacity) {
    vm->gray_list_capacity *= 2;
    vm->gray_list = (Object**)vm->config.realloc_fn(
      vm->gray_list,
      vm->gray_list_capacity * sizeof(Object*),
      vm->config.user_data);
  }

  vm->gray_list[vm->gray_list_count++] = self;
}

void grayValue(PKVM* vm, Var self) {
  if (!IS_OBJ(self)) return;
  grayObject(vm, AS_OBJ(self));
}

void grayVarBuffer(PKVM* vm, VarBuffer* self) {
  if (self == NULL) return;
  for (uint32_t i = 0; i < self->count; i++) {
    grayValue(vm, self->data[i]);
  }
}

#define GRAY_OBJ_BUFFER(m_name)                               \
  void gray##m_name##Buffer(PKVM* vm, m_name##Buffer* self) { \
    if (self == NULL) return;                                 \
    for (uint32_t i = 0; i < self->count; i++) {              \
      grayObject(vm, &self->data[i]->_super);                 \
    }                                                         \
  }

GRAY_OBJ_BUFFER(String)
GRAY_OBJ_BUFFER(Function)


static void blackenObject(Object* obj, PKVM* vm) {
  // TODO: trace here.

  switch (obj->type) {
    case OBJ_STRING: {
      vm->bytes_allocated += sizeof(String);
      vm->bytes_allocated += ((size_t)((String*)obj)->length + 1);
    } break;

    case OBJ_LIST: {
      List* list = (List*)obj;
      grayVarBuffer(vm, &list->elements);
      vm->bytes_allocated += sizeof(List);
      vm->bytes_allocated += sizeof(Var) * list->elements.capacity;
    } break;

    case OBJ_MAP: {
      Map* map = (Map*)obj;
      for (uint32_t i = 0; i < map->capacity; i++) {
        if (IS_UNDEF(map->entries[i].key)) continue;
        grayValue(vm, map->entries[i].key);
        grayValue(vm, map->entries[i].value);
      }
      vm->bytes_allocated += sizeof(Map);
      vm->bytes_allocated += sizeof(MapEntry) * map->capacity;
    } break;

    case OBJ_RANGE: {
      vm->bytes_allocated += sizeof(Range);
    } break;

    case OBJ_SCRIPT:
    {
      Script* scr = (Script*)obj;
      vm->bytes_allocated += sizeof(Script);

      grayObject(vm, &scr->path->_super);
      grayObject(vm, &scr->moudle->_super);

      grayVarBuffer(vm, &scr->globals);
      vm->bytes_allocated += sizeof(Var) * scr->globals.capacity;

      // Integer buffer have no gray call.
      vm->bytes_allocated += sizeof(uint32_t) * scr->global_names.capacity;

      grayVarBuffer(vm, &scr->literals);
      vm->bytes_allocated += sizeof(Var) * scr->literals.capacity;

      grayFunctionBuffer(vm, &scr->functions);
      vm->bytes_allocated += sizeof(Function*) * scr->functions.capacity;

      // Integer buffer have no gray call.
      vm->bytes_allocated += sizeof(uint32_t) * scr->function_names.capacity;

      grayStringBuffer(vm, &scr->names);
      vm->bytes_allocated += sizeof(String*) * scr->names.capacity;

      grayObject(vm, &scr->body->_super);
    } break;

    case OBJ_FUNC:
    {
      Function* func = (Function*)obj;
      vm->bytes_allocated += sizeof(Function);

      grayObject(vm, &func->owner->_super);

      if (!func->is_native) {
        Fn* fn = func->fn;
        vm->bytes_allocated += sizeof(uint8_t)* fn->opcodes.capacity;
        vm->bytes_allocated += sizeof(int) * fn->oplines.capacity;
      }
    } break;

    case OBJ_FIBER:
    {
      Fiber* fiber = (Fiber*)obj;
      vm->bytes_allocated += sizeof(Fiber);

      grayObject(vm, &fiber->func->_super);

      // Blacken the stack.
      for (Var* local = fiber->stack; local < fiber->sp; local++) {
        grayValue(vm, *local);
      }
      vm->bytes_allocated += sizeof(Var) * fiber->stack_size;

      // Blacken call frames.
      for (int i = 0; i < fiber->frame_count; i++) {
        grayObject(vm, (Object*)&fiber->frames[i].fn->_super);
        grayObject(vm, &fiber->frames[i].fn->owner->_super);
      }
      vm->bytes_allocated += sizeof(CallFrame) * fiber->frame_capacity;

      grayObject(vm, &fiber->caller->_super);
      grayObject(vm, &fiber->error->_super);

    } break;

    case OBJ_USER:
      TODO;
      break;
  }
}


void blackenObjects(PKVM* vm) {
  while (vm->gray_list_count > 0) {
    // Pop the gray object from the list.
    Object* gray = vm->gray_list[--vm->gray_list_count];
    blackenObject(gray, vm);
  }
}

Var doubleToVar(double value) {
#if VAR_NAN_TAGGING
  return utilDoubleToBits(value);
#else
#error TODO:
#endif // VAR_NAN_TAGGING
}

double varToDouble(Var value) {
#if VAR_NAN_TAGGING
  return utilDoubleFromBits(value);
#else
  #error TODO:
#endif // VAR_NAN_TAGGING
}

static String* _allocateString(PKVM* vm, size_t length) {
  String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
  varInitObject(&string->_super, vm, OBJ_STRING);
  string->length = (uint32_t)length;
  string->data[length] = '\0';
  string->capacity = (uint32_t)(length + 1);
  return string;
}

String* newStringLength(PKVM* vm, const char* text, uint32_t length) {

  ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

  String* string = _allocateString(vm, length);

  if (length != 0 && text != NULL) memcpy(string->data, text, length);
  string->hash = utilHashString(string->data);

  return string;
}

List* newList(PKVM* vm, uint32_t size) {
  List* list = ALLOCATE(vm, List);
  vmPushTempRef(vm, &list->_super);
  varInitObject(&list->_super, vm, OBJ_LIST);
  varBufferInit(&list->elements);
  if (size > 0) {
    varBufferFill(&list->elements, vm, VAR_NULL, size);
    list->elements.count = 0;
  }
  vmPopTempRef(vm);
  return list;
}

Map* newMap(PKVM* vm) {
  Map* map = ALLOCATE(vm, Map);
  varInitObject(&map->_super, vm, OBJ_MAP);
  map->capacity = 0;
  map->count = 0;
  map->entries = NULL;
  return map;
}

Range* newRange(PKVM* vm, double from, double to) {
  Range* range = ALLOCATE(vm, Range);
  varInitObject(&range->_super, vm, OBJ_RANGE);
  range->from = from;
  range->to = to;
  return range;
}

Script* newScript(PKVM* vm, String* path) {
  Script* script = ALLOCATE(vm, Script);
  varInitObject(&script->_super, vm, OBJ_SCRIPT);

  script->path = path;
  script->moudle = NULL;
  script->initialized = false;

  varBufferInit(&script->globals);
  uintBufferInit(&script->global_names);
  varBufferInit(&script->literals);
  functionBufferInit(&script->functions);
  uintBufferInit(&script->function_names);
  stringBufferInit(&script->names);

  vmPushTempRef(vm, &script->_super);
  const char* fn_name = "$(SourceBody)";
  script->body = newFunction(vm, fn_name, (int)strlen(fn_name), script, false);
  vmPopTempRef(vm);

  return script;
}

Function* newFunction(PKVM* vm, const char* name, int length, Script* owner,
                      bool is_native) {

  Function* func = ALLOCATE(vm, Function);
  varInitObject(&func->_super, vm, OBJ_FUNC);

  if (owner == NULL) {
    ASSERT(is_native, OOPS);
    func->name = name;
    func->owner = NULL;
    func->is_native = is_native;

  } else {
    // Add the name in the script's function buffer.
    vmPushTempRef(vm, &func->_super);
    functionBufferWrite(&owner->functions, vm, func);
    uint32_t name_index = scriptAddName(owner, vm, name, length);
    uintBufferWrite(&owner->function_names, vm, name_index);
    vmPopTempRef(vm);
  
    func->name = owner->names.data[name_index]->data;
    func->owner = owner;
    func->arity = -2; // -1 means variadic args.
    func->is_native = is_native;
  }

  if (is_native) {
    func->native = NULL;
  } else {
    Fn* fn = ALLOCATE(vm, Fn);

    byteBufferInit(&fn->opcodes);
    uintBufferInit(&fn->oplines);
    fn->stack_size = 0;
    func->fn = fn;
  }
  return func;
}

Fiber* newFiber(PKVM* vm, Function* fn) {
  Fiber* fiber = ALLOCATE(vm, Fiber);
  memset(fiber, 0, sizeof(Fiber));
  varInitObject(&fiber->_super, vm, OBJ_FIBER);

  fiber->state = FIBER_NEW;
  fiber->func = fn;

  if (fn->is_native) {
    // For native functions we're only using stack for parameters, there wont
    // be any locals or temps (which are belongs to the native "C" stack).
    int stack_size = utilPowerOf2Ceil(fn->arity + 1);
    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    fiber->stack_size = stack_size;
    fiber->sp = fiber->stack;
    fiber->ret = fiber->stack;

  } else {
    // Allocate stack.
    int stack_size = utilPowerOf2Ceil(fn->fn->stack_size + 1);
    if (stack_size < MIN_STACK_SIZE) stack_size = MIN_STACK_SIZE;
    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    fiber->stack_size = stack_size;
    fiber->sp = fiber->stack;
    fiber->ret = fiber->stack;

    // Allocate call frames.
    fiber->frame_capacity = INITIAL_CALL_FRAMES;
    fiber->frames = ALLOCATE_ARRAY(vm, CallFrame, fiber->frame_capacity);
    fiber->frame_count = 1;

    // Initialize the first frame.
    fiber->frames[0].fn = fn;
    fiber->frames[0].ip = fn->fn->opcodes.data;
    fiber->frames[0].rbp = fiber->ret;
  }

  return fiber;
}

void listInsert(PKVM* vm, List* self, uint32_t index, Var value) {

  // Add an empty slot at the end of the buffer.
  if (IS_OBJ(value)) vmPushTempRef(vm, AS_OBJ(value));
  varBufferWrite(&self->elements, vm, VAR_NULL);
  if (IS_OBJ(value)) vmPopTempRef(vm);

  // Shift the existing elements down.
  for (uint32_t i = self->elements.count - 1; i > index; i--) {
    self->elements.data[i] = self->elements.data[i - 1];
  }

  // Insert the new element.
  self->elements.data[index] = value;
}

Var listRemoveAt(PKVM* vm, List* self, uint32_t index) {
  Var removed = self->elements.data[index];
  if (IS_OBJ(removed)) vmPushTempRef(vm, AS_OBJ(removed));

  // Shift the rest of the elements up.
  for (uint32_t i = index; i < self->elements.count - 1; i++) {
    self->elements.data[i] = self->elements.data[i + 1];
  }

  // Shrink the size if it's too much excess.
  if (self->elements.capacity / GROW_FACTOR >= self->elements.count) {
    self->elements.data = (Var*)vmRealloc(vm, self->elements.data,
      sizeof(Var) * self->elements.capacity,
      sizeof(Var) * self->elements.capacity / GROW_FACTOR);
    self->elements.capacity /= GROW_FACTOR;
  }

  if (IS_OBJ(removed)) vmPopTempRef(vm);

  self->elements.count--;
  return removed;
}

// Return a has value for the object.
static uint32_t _hashObject(Object* obj) {
  
  ASSERT(isObjectHashable(obj->type),
         "Check if it's hashable before calling this method.");

  switch (obj->type) {

    case OBJ_STRING:
      return ((String*)obj)->hash;

    case OBJ_LIST:
    case OBJ_MAP:
      goto L_unhashable;

    case OBJ_RANGE:
    {
      Range* range = (Range*)obj;
      return utilHashNumber(range->from) ^ utilHashNumber(range->to);
    }

    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_USER:
      TODO;

    default:
    L_unhashable:
      UNREACHABLE();
      break;
  }

  UNREACHABLE();
  return -1;
}

uint32_t varHashValue(Var v) {
  if (IS_OBJ(v)) return _hashObject(AS_OBJ(v));

#if VAR_NAN_TAGGING
  return utilHashBits(v);
#else
  #error TODO:
#endif
}

// Find the entry with the [key]. Returns true if found and set [result] to
// point to the entry, return false otherwise and points [result] to where
// the entry should be inserted.
static bool _mapFindEntry(Map* self, Var key, MapEntry** result) {

  // An empty map won't contain the key.
  if (self->capacity == 0) return false;

  // The [start_index] is where the entry supposed to be if there wasn't any
  // collision occured. It'll be the start index for the linear probing.
  uint32_t start_index = varHashValue(key) % self->capacity;
  uint32_t index = start_index;

  // Keep track of the first tombstone after the [start_index] if we don't find
  // the key anywhere. The tombstone would be the entry at where we will have
  // to insert the key/value pair.
  MapEntry* tombstone = NULL;

  do {
    MapEntry* entry = &self->entries[index];

    if (IS_UNDEF(entry->key)) {
      ASSERT(IS_BOOL(entry->value), OOPS);

      if (IS_TRUE(entry->value)) {

        // We've found a tombstone, if we haven't found one [tombstone] should
        // be updated. We still need to keep search for if the key exists.
        if (tombstone == NULL) tombstone = entry;

      } else {
        // We've found a new empty slot and the key isn't found. If we've
        // found a tombstone along the sequence we could use that entry
        // otherwise the entry at the current index.

        *result = (tombstone != NULL) ? tombstone : entry;
        return false;
      }

    } else if (isValuesEqual(entry->key, key)) {
      // We've found the key.
      *result = entry;
      return true;
    }
    
    index = (index + 1) % self->capacity;

  } while (index != start_index);

  // If we reach here means the map is filled with tombstone. Set the first
  // tombstone as result for the next insertion and return false.
  ASSERT(tombstone != NULL, OOPS);
  *result = tombstone;
  return false;
}

// Add the key, value pair to the entries array of the map. Returns true if 
// the entry added for the first time and false for replaced vlaue.
static bool _mapInsertEntry(Map* self, Var key, Var value) {

  ASSERT(self->capacity != 0, "Should ensure the capacity before inserting.");

  MapEntry* result;
  if (_mapFindEntry(self, key, &result)) {
    // Key already found, just replace the value.
    result->value = value;
    return false;
  } else {
    result->key = key;
    result->value = value;
    return true;
  }
}

// Resize the map's size to the given [capacity].
static void _mapResize(PKVM* vm, Map* self, uint32_t capacity) {

  MapEntry* old_entries = self->entries;
  uint32_t old_capacity = self->capacity;

  self->entries = ALLOCATE_ARRAY(vm, MapEntry, capacity);
  self->capacity = capacity;
  for (uint32_t i = 0; i < capacity; i++) {
    self->entries[i].key = VAR_UNDEFINED;
    self->entries[i].value = VAR_FALSE;
  }

  // Insert the old entries to the new entries.
  for (uint32_t i = 0; i < old_capacity; i++) {
    // Skip the empty entries or tombstones.
    if (IS_UNDEF(old_entries[i].key)) continue;

    _mapInsertEntry(self, old_entries[i].key, old_entries[i].value);
  }

  DEALLOCATE(vm, old_entries);
}

Var mapGet(Map* self, Var key) {
  MapEntry* entry;
  if (_mapFindEntry(self, key, &entry)) return entry->value;
  return VAR_UNDEFINED;
}

void mapSet(PKVM* vm, Map* self, Var key, Var value) {

  // If map is about to fill, resize it first.
  if (self->count + 1 > self->capacity * MAP_LOAD_PERCENT / 100) {
    uint32_t capacity = self->capacity * GROW_FACTOR;
    if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
    _mapResize(vm, self, capacity);
  }

  if (_mapInsertEntry(self, key, value)) {
    self->count++; //< A new key added.
  }
}

void mapClear(PKVM* vm, Map* self) {
  DEALLOCATE(vm, self->entries);
  self->entries = NULL;
  self->capacity = 0;
  self->count = 0;
}

Var mapRemoveKey(PKVM* vm, Map* self, Var key) {
  MapEntry* entry;
  if (!_mapFindEntry(self, key, &entry)) return VAR_NULL;

  // Set the key as VAR_UNDEFINED to mark is as an available slow and set it's
  // value to VAR_TRUE for tombstone.
  Var value = entry->value;
  entry->key = VAR_UNDEFINED;
  entry->value = VAR_TRUE;

  self->count--;

  if (IS_OBJ(value)) vmPushTempRef(vm, AS_OBJ(value));

  if (self->count == 0) {
    // Clear the map if it's empty.
    mapClear(vm, self);

 } else if ((self->capacity > MIN_CAPACITY) &&
             (self->capacity / (GROW_FACTOR * GROW_FACTOR)) >
             ((self->count * 100) / MAP_LOAD_PERCENT)) {

    // We grow the map when it's filled 75% (MAP_LOAD_PERCENT) by 2
    // (GROW_FACTOR) but we're not shrink the map when it's half filled (ie.
    // half of the capacity is 75%). Instead we wait till it'll become 1/4 is
    // filled (1/4 = 1/(GROW_FACTOR*GROW_FACTOR)) to minimize the
    // reallocations and which is more faster.

    uint32_t capacity = self->capacity / (GROW_FACTOR * GROW_FACTOR);
    if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;

    _mapResize(vm, self, capacity);
  }

  if (IS_OBJ(value)) vmPopTempRef(vm);

  return value;
}

bool fiberHasError(Fiber* fiber) {
  return fiber->error != NULL;
}

void freeObject(PKVM* vm, Object* self) {
  // TODO: Debug trace memory here.

  // First clean the object's referencs, but we're not recursively doallocating
  // them because they're not marked and will be cleaned later.
  // Example: List's `elements` is VarBuffer that contain a heap allocated
  // array of `var*` which will be cleaned below but the actual `var` elements
  // will won't be freed here instead they havent marked at all, and will be
  // removed at the sweeping phase of the garbage collection.
  switch (self->type) {
    case OBJ_STRING:
      break;

    case OBJ_LIST:
      varBufferClear(&(((List*)self)->elements), vm);
      break;

    case OBJ_MAP:
      DEALLOCATE(vm, ((Map*)self)->entries);
      break;

    case OBJ_RANGE:
      break;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)self;
      varBufferClear(&scr->globals, vm);
      uintBufferClear(&scr->global_names, vm);
      varBufferClear(&scr->literals, vm);
      functionBufferClear(&scr->functions, vm);
      uintBufferClear(&scr->function_names, vm);
      stringBufferClear(&scr->names, vm);
    } break;

    case OBJ_FUNC: {
      Function* func = (Function*)self;
      if (!func->is_native) {
        byteBufferClear(&func->fn->opcodes, vm);
        uintBufferClear(&func->fn->oplines, vm);
      }
    } break;

    case OBJ_FIBER: {
      Fiber* fiber = (Fiber*)self;
      DEALLOCATE(vm, fiber->stack);
      DEALLOCATE(vm, fiber->frames);
    } break;

    case OBJ_USER:
      TODO; // Remove OBJ_USER.
      break;
  }

  DEALLOCATE(vm, self);
}

// Utility functions //////////////////////////////////////////////////////////

const char* getPkVarTypeName(PkVarType type) {
  switch (type) {
    case PK_NULL:     return "null";
    case PK_BOOL:     return "bool";
    case PK_NUMBER:   return "number";
    case PK_STRING:   return "String";
    case PK_LIST:     return "List";
    case PK_MAP:      return "Map";
    case PK_RANGE:    return "Range";
    case PK_SCRIPT:   return "Script";
    case PK_FUNCTION: return "Function";
    case PK_FIBER:    return "Fiber";
  }

  UNREACHABLE();
  return NULL;
}

const char* getObjectTypeName(ObjectType type) {
  switch (type) {
    case OBJ_STRING:  return "String";
    case OBJ_LIST:    return "List";
    case OBJ_MAP:     return "Map";
    case OBJ_RANGE:   return "Range";
    case OBJ_SCRIPT:  return "Script";
    case OBJ_FUNC:    return "Func";
    case OBJ_FIBER:   return "Fiber";
    case OBJ_USER:    return "UserObj";
    default:
      UNREACHABLE();
  }
}

const char* varTypeName(Var v) {
  if (IS_NULL(v)) return "null";
  if (IS_BOOL(v)) return "bool";
  if (IS_NUM(v))  return "number";

  ASSERT(IS_OBJ(v), OOPS);
  Object* obj = AS_OBJ(v);
  return getObjectTypeName(obj->type);
}

bool isValuesSame(Var v1, Var v2) {
#if VAR_NAN_TAGGING
  // Bit representation of each values are unique so just compare the bits.
  return v1 == v2;
#else
  #error TODO:
#endif
}

bool isValuesEqual(Var v1, Var v2) {
  if (isValuesSame(v1, v2)) return true;

  // If we reach here only heap allocated objects could be compared.
  if (!IS_OBJ(v1) || !IS_OBJ(v2)) return false;

  Object* o1 = AS_OBJ(v1), *o2 = AS_OBJ(v2);
  if (o1->type != o2->type) return false;

  switch (o1->type) {
    case OBJ_RANGE:
      return ((Range*)o1)->from == ((Range*)o2)->from &&
             ((Range*)o1)->to   == ((Range*)o2)->to;

    case OBJ_STRING: {
      String* s1 = (String*)o1, *s2 = (String*)o2;
      return s1->hash == s2->hash &&
             s1->length == s2->length &&
             memcmp(s1->data, s2->data, s1->length) == 0;
    }

    case OBJ_LIST: {
      /*
      * l1 = []; list_append(l1, l1) # [[...]]
      * l2 = []; list_append(l2, l2) # [[...]]
      * l1 == l2 ## This will cause a stack overflow but not handling that
      *          ## (in python too).
      */
      List *l1 = (List*)o1, *l2 = (List*)o2;
      if (l1->elements.count != l2->elements.count) return false;
      Var* v1 = l1->elements.data;
      Var* v2 = l2->elements.data;
      for (uint32_t i = 0; i < l1->elements.count; i++) {
        if (!isValuesEqual(*v1, *v2)) return false;
        v1++, v2++;
      }
      return true;
    }

    default:
      return false;
  }
}

bool isObjectHashable(ObjectType type) {
  // Only list and map are un-hashable.
  return type != OBJ_LIST && type != OBJ_MAP;
}

// This will prevent recursive list/map from crash when calling to_string, by
// checking if the current sequence is in the outer sequence linked list.
struct OuterSequence {
  struct OuterSequence* outer;
  // If false it'll be map. If we have multiple sequence we should use an enum
  // here but we only ever support list and map as builtin sequence (so bool).
  bool is_list;
  union {
    const List* list;
    const Map* map;
  };
};
typedef struct OuterSequence OuterSequence;

static void toStringInternal(PKVM* vm, Var v, ByteBuffer* buff,
                             OuterSequence* outer) {
  if (IS_NULL(v)) {
    byteBufferAddString(buff, vm, "null", 4);
    return;

  } else if (IS_BOOL(v)) {
    if (AS_BOOL(v)) byteBufferAddString(buff, vm, "true", 4);
    else byteBufferAddString(buff, vm, "false", 5);
    return;

  } else if (IS_NUM(v)) {
    char num_buff[TO_STRING_BUFF_SIZE];
    int length = sprintf(num_buff, "%.14g", AS_NUM(v));
    __ASSERT(length < TO_STRING_BUFF_SIZE, "Buffer overflowed.");
    byteBufferAddString(buff, vm, num_buff, length); return;

  } else if (IS_OBJ(v)) {

    const Object* obj = AS_OBJ(v);
    switch (obj->type) {

      case OBJ_STRING:
      {
        const String* str = (const String*)obj;
        if (outer == NULL) {
          byteBufferAddString(buff, vm, str->data, str->length);
          return;
        }

        // If recursive return with quotes (ex: [42, "hello", 0..10]).
        byteBufferWrite(buff, vm, '"');
        byteBufferAddString(buff, vm, str->data, str->length);
        byteBufferWrite(buff, vm, '"');
        return;
      }

      case OBJ_LIST:
      {
        const List* list = (const List*)obj;
        if (list->elements.count == 0) {
          byteBufferAddString(buff, vm, "[]", 2);
          return;
        }

        // Check if the list is recursive.
        OuterSequence* seq = outer;
        while (seq != NULL) {
          if (seq->is_list) {
            if (seq->list == list) {
              byteBufferAddString(buff, vm, "[...]", 5);
              return;
            }
          }
          seq = seq->outer;
        }
        OuterSequence seq_list;
        seq_list.outer = outer; seq_list.is_list = true; seq_list.list = list;

        byteBufferWrite(buff, vm, '[');
        for (uint32_t i = 0; i < list->elements.count; i++) {
          if (i != 0) byteBufferAddString(buff, vm, ", ", 2);
          toStringInternal(vm, list->elements.data[i], buff, &seq_list);
        }
        byteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_MAP:
      {
        const Map* map = (const Map*)obj;
        if (map->entries == NULL) {
          byteBufferAddString(buff, vm, "{}", 2);
          return;
        }

        // Check if the map is recursive.
        OuterSequence* seq = outer;
        while (seq != NULL) {
          if (!seq->is_list) {
            if (seq->map == map) {
              byteBufferAddString(buff, vm, "{...}", 5);
              return;
            }
          }
          seq = seq->outer;
        }
        OuterSequence seq_map;
        seq_map.outer = outer; seq_map.is_list = false; seq_map.map = map;

        byteBufferWrite(buff, vm, '{');
        uint32_t i = 0;     // Index of the current entry to iterate.
        bool _first = true; // For first element no ',' required.
        do {

          // Get the next valid key index.
          bool _done = false;
          while (IS_UNDEF(map->entries[i].key)) {
            if (++i >= map->capacity) {
              _done = true;
              break;
            }
          }
          if (_done) break;

          if (!_first) {
            byteBufferAddString(buff, vm, ", ", 2);
            _first = false;
          }
          toStringInternal(vm, map->entries[i].key, buff, &seq_map);
          byteBufferWrite(buff, vm, ':');
          toStringInternal(vm, map->entries[i].value, buff, &seq_map);
          i++;
        } while (i < map->capacity);

        byteBufferWrite(buff, vm, '}');
        return;
      }

      case OBJ_RANGE:
      {
        const Range* range = (const Range*)obj;

        char buff_from[STR_NUM_BUFF_SIZE];
        const int len_from = snprintf(buff_from, sizeof(buff_from), "%f",
                                      range->from);
        char buff_to[STR_NUM_BUFF_SIZE];
        const int len_to = snprintf(buff_to, sizeof(buff_to), "%f", range->to);
        byteBufferAddString(buff, vm, "[Range:", 7);
        byteBufferAddString(buff, vm, buff_from, len_from);
        byteBufferAddString(buff, vm, "..", 2);
        byteBufferAddString(buff, vm, buff_to, len_to);
        byteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_SCRIPT: {
        const Script* scr = (const Script*)obj;
        byteBufferAddString(buff, vm, "[Module:", 8);
        if (scr->moudle != NULL) {
          byteBufferAddString(buff, vm, scr->moudle->data,
                              scr->moudle->length);
        } else {
          byteBufferWrite(buff, vm, '"');
          byteBufferAddString(buff, vm, scr->path->data, scr->path->length);
          byteBufferWrite(buff, vm, '"');
        }
        byteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_FUNC: {
        const Function* fn = (const Function*)obj;
        byteBufferAddString(buff, vm, "[Func:", 6);
        byteBufferAddString(buff, vm, fn->name, (uint32_t)strlen(fn->name));
        byteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_FIBER: {
        const Fiber* fb = (const Fiber*)obj;
        byteBufferAddString(buff, vm, "[Fiber:", 7);
        byteBufferAddString(buff, vm, fb->func->name, strlen(fb->func->name));
        byteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_USER: {
        // TODO:
        byteBufferAddString(buff, vm, "[UserObj]", 9);
        return;
      }
    }

  }
  UNREACHABLE();
  return;
}

String* toString(PKVM* vm, Var v) {
  ByteBuffer buff;
  byteBufferInit(&buff);
  toStringInternal(vm, v, &buff, NULL);
  return newStringLength(vm, (const char*)buff.data, buff.count);
}

bool toBool(Var v) {

  if (IS_BOOL(v)) return AS_BOOL(v);
  if (IS_NULL(v)) return false;
  if (IS_NUM(v)) return AS_NUM(v) != 0;

  ASSERT(IS_OBJ(v), OOPS);
  Object* o = AS_OBJ(v);
  switch (o->type) {
    case OBJ_STRING: return ((String*)o)->length != 0;
    case OBJ_LIST:   return ((List*)o)->elements.count != 0;
    case OBJ_MAP:    return ((Map*)o)->count != 0;
    case OBJ_RANGE: // [[FALLTHROUGH]]
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_USER:
      return true;
    default:
      UNREACHABLE();
  }

  return true;
}

String* stringFormat(PKVM* vm, const char* fmt, ...) {
  va_list arg_list;

  // Calculate the total length of the resulting string. This is required to 
  // determine the final string size to allocate.
  va_start(arg_list, fmt);
  size_t total_length = 0;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
        total_length += strlen(va_arg(arg_list, const char*));
        break;

      case '@':
        total_length += va_arg(arg_list, String*)->length;
        break;

      default:
        total_length++;
    }
  }
  va_end(arg_list);

  // Now build the new string.
  String* result = _allocateString(vm, total_length);
  va_start(arg_list, fmt);
  char* buff = result->data;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
      {
        const char* string = va_arg(arg_list, const char*);
        size_t length = strlen(string);
        memcpy(buff, string, length);
        buff += length;
      } break;

      case '@':
      {
        String* string = va_arg(arg_list, String*);
        memcpy(buff, string->data, string->length);
        buff += string->length;
      } break;

      default:
      {
        *buff++ = *c;
      } break;
    }
  }
  va_end(arg_list);

  result->hash = utilHashString(result->data);
  return result;
}

String* stringJoin(PKVM* vm, String* str1, String* str2) {

  // Optimize end case.
  if (str1->length == 0) return str2;
  if (str2->length == 0) return str1;

  size_t length = (size_t)str1->length + (size_t)str2->length;
  String* string = _allocateString(vm, length);

  memcpy(string->data,                str1->data, str1->length);
  memcpy(string->data + str1->length, str2->data, str2->length);
  // Null byte already existed. From _allocateString.

  string->hash = utilHashString(string->data);
  return string;
}

uint32_t scriptAddName(Script* self, PKVM* vm, const char* name,
  uint32_t length) {

  for (uint32_t i = 0; i < self->names.count; i++) {
    String* _name = self->names.data[i];
    if (_name->length == length && strncmp(_name->data, name, length) == 0) {
      // Name already exists in the buffer.
      return i;
    }
  }

  // If we reach here the name doesn't exists in the buffer, so add it and
  // return the index.
  String* new_name = newStringLength(vm, name, length);
  vmPushTempRef(vm, &new_name->_super);
  stringBufferWrite(&self->names, vm, new_name);
  vmPopTempRef(vm);
  return self->names.count - 1;
}

int scriptSearchFunc(Script* script, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < script->function_names.count; i++) {
    uint32_t name_index = script->function_names.data[i];
    String* fn_name = script->names.data[name_index];
    if (fn_name->length == length &&
      strncmp(fn_name->data, name, length) == 0) {
      return i;
    }
  }
  return -1;
}

int scriptSearchGlobals(Script* script, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < script->global_names.count; i++) {
    uint32_t name_index = script->global_names.data[i];
    String* g_name = script->names.data[name_index];
    if (g_name->length == length && strncmp(g_name->data, name, length) == 0) {
      return i;
    }
  }
  return -1;
}
