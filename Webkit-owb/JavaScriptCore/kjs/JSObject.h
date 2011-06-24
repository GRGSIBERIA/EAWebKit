// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

/*
* This file was modified by Electronic Arts Inc Copyright � 2009
*/

#ifndef KJS_OBJECT_H
#define KJS_OBJECT_H

#include <wtf/FastAllocBase.h>
#include "CommonIdentifiers.h"
#include "ExecState.h"
#include "JSType.h"
#include "list.h"
#include "PropertyMap.h"
#include "PropertySlot.h"
#include "ScopeChain.h"

namespace KJS {

  class InternalFunction;
  class PropertyNameArray;

  struct HashEntry;
  struct HashTable;

  // ECMA 262-3 8.6.1
  // Property attributes
  enum Attribute { None         = 0,
                   ReadOnly     = 1 << 1, // property can be only read, not written
                   DontEnum     = 1 << 2, // property doesn't appear in (for .. in ..)
                   DontDelete   = 1 << 3, // property can't be deleted
                   Function     = 1 << 4, // property is a function - only used by static hashtables
                   IsGetterSetter = 1 << 5 }; // property is a getter or setter

  /**
   * Class Information
   */
  struct ClassInfo {
    /**
     * A string denoting the class name. Example: "Window".
     */
    const char* className;
    /**
     * Pointer to the class information of the base class.
     * 0L if there is none.
     */
    const ClassInfo* parentClass;
    /**
     * Static hash-table of properties.
     * For classes that can be used from multiple threads, it is accessed via a getter function that would typically return a pointer to thread-specific value.
     */
    const HashTable* propHashTable(ExecState* exec) const
    {
        if (classPropHashTableGetterFunction)
            return classPropHashTableGetterFunction(exec);
        return staticPropHashTable;
    }

    const HashTable* staticPropHashTable;
    typedef const HashTable* (*ClassPropHashTableGetterFunction)(ExecState*);
    const ClassPropHashTableGetterFunction classPropHashTableGetterFunction;
  };
  
  // This is an internal value object which stores getter and setter functions
  // for a property.
  class GetterSetter : public JSCell {
  public:
    JSType type() const { return GetterSetterType; }
      
    GetterSetter() : m_getter(0), m_setter(0) { }
      
    virtual void mark();
      
    JSObject* getter() const { return m_getter; }
    void setGetter(JSObject* getter) { m_getter = getter; }
    JSObject* setter() const { return m_setter; }
    void setSetter(JSObject* setter) { m_setter = setter; }
      
  private:
    virtual JSValue* toPrimitive(ExecState*, JSType preferred) const;
    virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
    virtual bool toBoolean(ExecState*) const;
    virtual double toNumber(ExecState*) const;
    virtual UString toString(ExecState*) const;
    virtual JSObject* toObject(ExecState*) const;

    JSObject* m_getter;
    JSObject* m_setter;  
  };
  
  class JSObject : public JSCell {
  public:
    /**
     * Creates a new JSObject with the specified prototype
     *
     * @param proto The prototype
     */
    JSObject(JSValue* proto);

    /**
     * Creates a new JSObject with a prototype of jsNull()
     * (that is, the ECMAScript "null" value, not a null object pointer).
     */
    JSObject();
    
    virtual void mark();
    virtual JSType type() const;

    /**
     * A pointer to a ClassInfo struct for this class. This provides a basic
     * facility for run-time type information, and can be used to check an
     * object's class an inheritance (see inherits()). This should
     * always return a statically declared pointer, or 0 to indicate that
     * there is no class information.
     *
     * This is primarily useful if you have application-defined classes that you
     * wish to check against for casting purposes.
     *
     * For example, to specify the class info for classes FooImp and BarImp,
     * where FooImp inherits from BarImp, you would add the following in your
     * class declarations:
     *
     * \code
     *   class BarImp : public JSObject {
     *     virtual const ClassInfo *classInfo() const { return &info; }
     *     static const ClassInfo info;
     *     // ...
     *   };
     *
     *   class FooImp : public JSObject {
     *     virtual const ClassInfo *classInfo() const { return &info; }
     *     static const ClassInfo info;
     *     // ...
     *   };
     * \endcode
     *
     * And in your source file:
     *
     * \code
     *   const ClassInfo BarImp::info = { "Bar", 0, 0, 0 }; // no parent class
     *   const ClassInfo FooImp::info = { "Foo", &BarImp::info, 0, 0 };
     * \endcode
     *
     * @see inherits()
     */

    /**
     * Checks whether this object inherits from the class with the specified
     * classInfo() pointer. This requires that both this class and the other
     * class return a non-NULL pointer for their classInfo() methods (otherwise
     * it will return false).
     *
     * For example, for two JSObject pointers obj1 and obj2, you can check
     * if obj1's class inherits from obj2's class using the following:
     *
     *   if (obj1->inherits(obj2->classInfo()))
     *     // ...
     *   }
     *
     * If you have a handle to a statically declared ClassInfo, such as in the
     * classInfo() example, you can check for inheritance without needing
     * an instance of the other class:
     *
     *   if (obj1->inherits(FooImp::info)) {
     *     // ...
     *   }
     *
     * @param cinfo The ClassInfo pointer for the class you want to check
     * inheritance against.
     * @return true if this object's class inherits from class with the
     * ClassInfo pointer specified in cinfo
     */
    bool inherits(const ClassInfo* classInfo) const { return isObject(classInfo); } // FIXME: Merge with isObject.

    // internal properties (ECMA 262-3 8.6.2)

    /**
     * Returns the prototype of this object. Note that this is not the same as
     * the "prototype" property.
     *
     * See ECMA 8.6.2
     *
     * @return The object's prototype
     */
    JSValue *prototype() const;
    void setPrototype(JSValue *proto);

    /**
     * Returns the class name of the object
     *
     * See ECMA 8.6.2
     *
     * @return The object's class name
     */
    /**
     * Implementation of the [[Class]] internal property (implemented by all
     * Objects)
     *
     * The default implementation uses classInfo().
     * You should either implement classInfo(), or
     * if you simply need a classname, you can reimplement className()
     * instead.
     */
    virtual UString className() const;

    /**
     * Retrieves the specified property from the object. If neither the object
     * or any other object in its prototype chain have the property, this
     * function will return Undefined.
     *
     * See ECMA 8.6.2.1
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to retrieve
     *
     * @return The specified property, or Undefined
     */
    JSValue* get(ExecState* exec, const Identifier& propertyName) const;
    JSValue* get(ExecState* exec, unsigned propertyName) const;

    bool getPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    bool getPropertySlot(ExecState*, unsigned, PropertySlot&);

    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    virtual bool getOwnPropertySlot(ExecState*, unsigned index, PropertySlot&);

    /**
     * Sets the specified property.
     *
     * See ECMA 8.6.2.2
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to set
     * @param propertyValue The value to set
     */
    virtual void put(ExecState*, const Identifier& propertyName, JSValue* value);
    virtual void put(ExecState*, unsigned propertyName, JSValue* value);

    virtual void putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes);
    virtual void putWithAttributes(ExecState*, unsigned propertyName, JSValue* value, unsigned attributes);

    /**
     * Checks if a property is enumerable, that is if it doesn't have the DontEnum
     * flag set
     *
     * See ECMA 15.2.4
     * @param exec The current execution state
     * @param propertyName The name of the property
     * @return true if the property is enumerable, otherwise false
     */
    bool propertyIsEnumerable(ExecState *exec, const Identifier &propertyName) const;

    /**
     * Checks to see whether the object (or any object in its prototype chain)
     * has a property with the specified name.
     *
     * See ECMA 8.6.2.4
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to check for
     * @return true if the object has the property, otherwise false
     */
    bool hasProperty(ExecState*, const Identifier&) const;
    bool hasProperty(ExecState*, unsigned) const;
    bool hasOwnProperty(ExecState*, const Identifier&) const;

    /**
     * Removes the specified property from the object.
     *
     * See ECMA 8.6.2.5
     *
     * @param exec The current execution state
     * @param propertyName The name of the property to delete
     * @return true if the property was successfully deleted or did not
     * exist on the object. false if deleting the specified property is not
     * allowed.
     */
    virtual bool deleteProperty(ExecState *exec, const Identifier &propertyName);
    virtual bool deleteProperty(ExecState *exec, unsigned propertyName);

    /**
     * Converts the object into a primitive value. The value return may differ
     * depending on the supplied hint
     *
     * See ECMA 8.6.2.6
     *
     * @param exec The current execution state
     * @param hint The desired primitive type to convert to
     * @return A primitive value converted from the objetc. Note that the
     * type of primitive value returned may not be the same as the requested
     * hint.
     */
    /**
     * Implementation of the [[DefaultValue]] internal property (implemented by
     * all Objects)
     */
    virtual JSValue* defaultValue(ExecState*, JSType hint) const;

    /**
     * Whether or not the object implements the hasInstance() method. If this
     * returns false you should not call the hasInstance() method on this
     * object (typically, an assertion will fail to indicate this).
     *
     * @return true if this object implements the hasInstance() method,
     * otherwise false
     */
    virtual bool implementsHasInstance() const;

    /**
     * Checks whether value delegates behavior to this object. Used by the
     * instanceof operator.
     *
     * @param exec The current execution state
     * @param value The value to check
     * @return true if value delegates behavior to this object, otherwise
     * false
     */
    virtual bool hasInstance(ExecState *exec, JSValue *value);

    virtual void getPropertyNames(ExecState*, PropertyNameArray&);

    virtual JSValue* toPrimitive(ExecState*, JSType preferredType = UnspecifiedType) const;
    virtual bool getPrimitiveNumber(ExecState*, double& number, JSValue*& value);
    virtual bool toBoolean(ExecState *exec) const;
    virtual double toNumber(ExecState *exec) const;
    virtual UString toString(ExecState *exec) const;
    virtual JSObject *toObject(ExecState *exec) const;

    virtual JSObject* toThisObject(ExecState*) const;
    virtual JSGlobalObject* toGlobalObject(ExecState*) const;

    virtual bool getPropertyAttributes(ExecState*, const Identifier& propertyName, unsigned& attributes) const;
    
    // WebCore uses this to make document.all and style.filter undetectable
    virtual bool masqueradeAsUndefined() const { return false; }
    
    // This get function only looks at the property map.
    // This is used e.g. by lookupOrCreateFunction (to cache a function, we don't want
    // to look up in the prototype, it might already exist there)
    JSValue *getDirect(const Identifier& propertyName) const
        { return _prop.get(propertyName); }
    JSValue **getDirectLocation(const Identifier& propertyName)
        { return _prop.getLocation(propertyName); }
   JSValue **getDirectLocation(const Identifier& propertyName, bool& isWriteable)
        { return _prop.getLocation(propertyName, isWriteable); }
    void putDirect(const Identifier &propertyName, JSValue *value, int attr = 0);
    void putDirect(ExecState*, const Identifier& propertyName, int value, int attr = 0);
    void removeDirect(const Identifier &propertyName);
    
    // convenience to add a function property under the function's own built-in name
    void putDirectFunction(InternalFunction*, int attr = 0);

    void fillGetterPropertySlot(PropertySlot& slot, JSValue **location);

    virtual void defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunction);
    virtual void defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunction);
    virtual JSValue* lookupGetter(ExecState*, const Identifier& propertyName);
    virtual JSValue* lookupSetter(ExecState*, const Identifier& propertyName);

    virtual bool isActivationObject() const { return false; }
    virtual bool isGlobalObject() const { return false; }
    virtual bool isVariableObject() const { return false; }

    virtual bool isWatchdogException() const { return false; }

  protected:
    PropertyMap _prop;
    bool getOwnPropertySlotForWrite(ExecState*, const Identifier&, PropertySlot&, bool& slotIsWriteable);

  private:
    const HashEntry* findPropertyHashEntry(ExecState*, const Identifier& propertyName) const;
    JSValue *_proto;
  };

    JSObject* constructEmptyObject(ExecState*);

  /**
   * Types of Native Errors available. For custom errors, GeneralError
   * should be used.
   */
  enum ErrorType { GeneralError   = 0,
                   EvalError      = 1,
                   RangeError     = 2,
                   ReferenceError = 3,
                   SyntaxError    = 4,
                   TypeError      = 5,
                   URIError       = 6};

  /**
   * @short Factory methods for error objects.
   */
  class Error {
public:
// Placement operator new.
void* operator new(size_t, void* p) { return p; }
void* operator new[](size_t, void* p) { return p; }
 
void* operator new(size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNew);
     return p;
}
 
void operator delete(void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNew);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
 
void* operator new[](size_t size)
{
     void* p = fastMalloc(size);
     fastMallocMatchValidateMalloc(p, WTF::Internal::AllocTypeClassNewArray);
     return p;
}
 
void operator delete[](void* p)
{
     fastMallocMatchValidateFree(p, WTF::Internal::AllocTypeClassNewArray);
     fastFree(p);  // We don't need to check for a null pointer; the compiler does this.
}
  public:
    /**
     * Factory method for error objects.
     *
     * @param exec The current execution state
     * @param errtype Type of error.
     * @param message Optional error message.
     * @param lineNumber Optional line number.
     * @param sourceId Optional source id.
     * @param sourceURL Optional source URL.
     */
    static JSObject *create(ExecState *, ErrorType, const UString &message, int lineNumber, int sourceId, const UString &sourceURL);
    static JSObject *create(ExecState *, ErrorType, const char *message);
  };

JSObject *throwError(ExecState *, ErrorType, const UString &message, int lineNumber, int sourceId, const UString &sourceURL);
JSObject *throwError(ExecState *, ErrorType, const UString &message);
JSObject *throwError(ExecState *, ErrorType, const char *message);
JSObject *throwError(ExecState *, ErrorType);

inline JSObject::JSObject(JSValue* proto)
    : _proto(proto)
{
    ASSERT(proto);
    ASSERT(proto == jsNull() || Heap::heap(this) == Heap::heap(proto));
}

inline JSObject::JSObject()
    : _proto(jsNull())
{
}

inline JSValue *JSObject::prototype() const
{
    return _proto;
}

inline void JSObject::setPrototype(JSValue *proto)
{
    ASSERT(proto);
    _proto = proto;
}

inline bool JSCell::isObject(const ClassInfo* info) const
{
    for (const ClassInfo* ci = classInfo(); ci; ci = ci->parentClass) {
        if (ci == info)
            return true;
    }
    return false;
}

// this method is here to be after the inline declaration of JSCell::isObject
inline bool JSValue::isObject(const ClassInfo* classInfo) const
{
    return !JSImmediate::isImmediate(this) && asCell()->isObject(classInfo);
}

inline JSValue *JSObject::get(ExecState *exec, const Identifier &propertyName) const
{
  PropertySlot slot(const_cast<JSObject *>(this));

  if (const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot))
    return slot.getValue(exec, propertyName);
    
  return jsUndefined();
}

inline JSValue *JSObject::get(ExecState *exec, unsigned propertyName) const
{
  PropertySlot slot(const_cast<JSObject *>(this));
  if (const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot))
    return slot.getValue(exec, propertyName);
    
  return jsUndefined();
}

// It may seem crazy to inline a function this large but it makes a big difference
// since this is function very hot in variable lookup
inline bool JSObject::getPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    JSObject *object = this;
    while (true) {
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;

        JSValue *proto = object->_proto;
        if (!proto->isObject())
            return false;

        object = static_cast<JSObject *>(proto);
    }
}

inline bool JSObject::getPropertySlot(ExecState *exec, unsigned propertyName, PropertySlot& slot)
{
  JSObject *imp = this;
  
  while (true) {
    if (imp->getOwnPropertySlot(exec, propertyName, slot))
      return true;
    
    JSValue *proto = imp->_proto;
    if (!proto->isObject())
      break;
    
    imp = static_cast<JSObject *>(proto);
  }
  
  return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlotForWrite(ExecState* exec, const Identifier& propertyName, PropertySlot& slot, bool& slotIsWriteable)
{
    if (JSValue **location = getDirectLocation(propertyName, slotIsWriteable)) {
        if (_prop.hasGetterSetterProperties() && location[0]->type() == GetterSetterType) {
            slotIsWriteable = false;
            fillGetterPropertySlot(slot, location);
        } else
            slot.setValueSlot(location);
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValueSlot(&_proto);
        slotIsWriteable = false;
        return true;
    }

    return false;
}

// It may seem crazy to inline a function this large, especially a virtual function,
// but it makes a big difference to property lookup that derived classes can inline their
// base class call to this.
ALWAYS_INLINE bool JSObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (JSValue **location = getDirectLocation(propertyName)) {
        if (_prop.hasGetterSetterProperties() && location[0]->type() == GetterSetterType)
            fillGetterPropertySlot(slot, location);
        else
            slot.setValueSlot(location);
        return true;
    }

    // non-standard Netscape extension
    if (propertyName == exec->propertyNames().underscoreProto) {
        slot.setValueSlot(&_proto);
        return true;
    }

    return false;
}

inline void JSObject::putDirect(const Identifier &propertyName, JSValue *value, int attr)
{
    _prop.put(propertyName, value, attr);
}

inline void JSObject::putDirect(ExecState* exec, const Identifier &propertyName, int value, int attr)
{
    _prop.put(propertyName, jsNumber(exec, value), attr);
}

inline JSValue* JSObject::toPrimitive(ExecState* exec, JSType preferredType) const
{
    return defaultValue(exec, preferredType);
}

inline JSValue* JSValue::get(ExecState* exec, const Identifier& propertyName) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        PropertySlot slot(const_cast<JSValue*>(this));
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = static_cast<JSCell*>(const_cast<JSValue*>(this));
    PropertySlot slot(cell);
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* proto = static_cast<JSObject*>(cell)->prototype();
        if (!proto->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(proto);
    }
}

inline JSValue* JSValue::get(ExecState* exec, unsigned propertyName) const
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSObject* prototype = JSImmediate::prototype(this, exec);
        PropertySlot slot(const_cast<JSValue*>(this));
        if (!prototype->getPropertySlot(exec, propertyName, slot))
            return jsUndefined();
        return slot.getValue(exec, propertyName);
    }
    JSCell* cell = const_cast<JSCell*>(asCell());
    PropertySlot slot(cell);
    while (true) {
        if (cell->getOwnPropertySlot(exec, propertyName, slot))
            return slot.getValue(exec, propertyName);
        ASSERT(cell->isObject());
        JSValue* proto = static_cast<JSObject*>(cell)->prototype();
        if (!proto->isObject())
            return jsUndefined();
        cell = static_cast<JSCell*>(proto);
    }
}

inline void JSValue::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

inline void JSValue::put(ExecState* exec, unsigned propertyName, JSValue* value)
{
    if (UNLIKELY(JSImmediate::isImmediate(this))) {
        JSImmediate::toObject(this, exec)->put(exec, propertyName, value);
        return;
    }
    asCell()->put(exec, propertyName, value);
}

} // namespace

#endif // KJS_OBJECT_H
