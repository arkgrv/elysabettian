//
//  object.c
//  Elysabettian
//
//  Created by Simone Rolando on 06/07/21.
//

#include "object.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#include <string.h>
#include <stdio.h>

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type);

static Object* allocate_object(int size, ObjType type)
{
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    object->next = vm.objects;
    vm.objects = object;
    
#ifdef DEBUG_LOG_GC
    printf("%p allocate %d for %d\n", (void*)object, size, type);
#endif
    return object;
}

ObjClass* new_class(ObjString* name)
{
    ObjClass* class = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
    class->name = name;
    init_table(&class->member_functions);
    return class;
}

ObjBoundFun* new_bound_fun(Value receiver, ObjClosure* function)
{
    ObjBoundFun* bound = ALLOCATE_OBJ(ObjBoundFun, OBJ_BOUND_FUN);
    bound->receiver = receiver;
    bound->function = function;
    return bound;
}

ObjClosure* new_closure(ObjFunction* function)
{
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalue_count);
    size_t i;
    for (i = 0; i < function->upvalue_count; ++i)
        upvalues[i] = NULL;
    
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjFunction* new_function()
{
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

ObjInstance* new_instance(ObjClass* class)
{
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->class = class;
    init_table(&instance->fields);
    return instance;
}

ObjNative* new_native(NativeFun function)
{
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = function;
    return native;
}

static ObjString* allocate_string(char* chars, int len, uint32_t hash)
{
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = len;
    string->chars = chars;
    string->hash = hash;
    
    push(OBJECT_VAL(string));
    table_set(&vm.strings, string, NULL_VAL);
    pop();
    return string;
}

static uint32_t hash_string(const char* key, int length)
{
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; i < length; ++i) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

ObjString* take_string(char* chars, int len)
{
    uint32_t hash = hash_string(chars, len);
    ObjString* interned = table_find_string(&vm.strings, chars, len, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, len + 1);
        return interned;
    }
    
    return allocate_string(chars, len, hash);
}

ObjString* copy_string(const char* chars, int len)
{
    uint32_t hash = hash_string(chars, len);
    ObjString* interned = table_find_string(&vm.strings, chars, len, hash);
    
    if (interned != NULL) return interned;
    
    char* heap_chars = ALLOCATE(char, len + 1);
    memcpy(heap_chars, chars, len);
    heap_chars[len] = '\0';
    
    return allocate_string(heap_chars, len, hash);
}

ObjUpvalue* new_upvalue(Value* slot)
{
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void print_function(ObjFunction* function)
{
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    
    printf("<fn %s>", function->name->chars);
}

void print_object(Value value)
{
    switch (OBJECT_VAL(value)) {
        case OBJ_BOUND_FUN:
            print_function(AS_BOUND_FUN(value)->function->function);
            break;
        case OBJ_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->class->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}