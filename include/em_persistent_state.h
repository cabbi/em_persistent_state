#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "EEPROM.h"

#include "em_list.h"

// Persistent State types definition
typedef uint16_t ps_size_t;
typedef uint16_t ps_address_t;

// Forward declaration
class EmPersistentId;
class EmPersistentState;
class EmPersistentValueBase;
class EmPersistentValueIterator;
bool _itemsMatch(const EmPersistentValueBase& pv1, 
                 const EmPersistentValueBase& pv2);

/***
    The persistent value list
***/
class EmPersistentValueList: public EmList<EmPersistentValueBase> {
public:
    EmPersistentValueList() : EmList<EmPersistentValueBase>(_itemsMatch) {}
};

/***
    The persistent state class stores values identified by a small id (i.e. 3 chars) into EEPROM.

    Usage example:

        EmPersistentState PS;
        EmPersistentUInt16 intVal = EmPersistentUInt16(PS, "i_v", 16);
        EmPersistentFloat floatVal = EmPersistentFloat(PS, "f_v", 55.3);
        EmPersistentString textVal = EmPersistentString(PS, "txt", 10, "Hello!");

        void setup() {
            // First way to initialize Persistent State
            EmPersistentValueList values;
            values.Append(floatVal);
            values.Append(intVal);
            values.Append(textVal);
            PS.Init(values, false);
        }

        void setup_() {
            // Second way to initialize Persistent State
            // NOTE: this way we cannot delete old unused values but we 
            //       spare memory by not filling a 'PersistentValueList' 
            if (PS.Init()) {
                PS.Add(floatVal);
                PS.Add(intVal);
                PS.Add(textVal);
            }
        }

        int main() {
            setup();

            // Storing new values to PS
            textVal = "Got new value!"; // This will be truncated because of max len of 10!
            intVal = 44;
            
            return 0;
        }
***/
class EmPersistentState {
    friend class EmPersistentValueBase;
    friend class EmPersistentId;
public:    
    const static EmPersistentId c_HeaderId; 
    const static EmPersistentId c_FooterId;
    const static int c_MinSize = 12;

    EmPersistentState(ps_address_t beginIndex = EEPROM.begin(),
                      ps_address_t endIndex = EEPROM.end());

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentState() {
    }

    // Initialize the persistent state without changing current stored values.
    bool Init();

    // Initialize the persistent state by loading 'values' list, 
    // setting current values to the requested 'values'.
    //
    // If 'removeUnusedValues' is set to true the storage will remove 
    // values that are not in the desired 'values' list.
    //
    // NOTE:
    //   appending values to the 'values' list after the 'Init' call has no effect.
    //   Use the persistent state 'Add' method to add values to it.
    bool Init(const EmPersistentValueList& values, bool removeUnusedValues);

    // Checks if persistent state has been initialized (i.e. 'Init' call!)
    bool IsInitialized() const {
        return 0 != m_NextPvAddress;
    }

    // Load the current persistent values into a list
    bool Load(EmPersistentValueList& values);

    // Iterate the persistent values one by one without generating a full elements list
    bool Iterate(EmPersistentValueIterator& iterator);

    // Add a value to storage. 
    // This method will check if 'value' is already stored and set its 
    // current value taken from persistent state.
    bool Add(EmPersistentValueBase& value);

    // Clear the PS by resetting all its stored values
    bool Clear();

protected:   
    // Initialize the persistent state for the very first time (i.e. writing header and footer ids)
    bool _init();

    // Append a new value to storage
    bool _appendValue(EmPersistentValueBase* pValue);

    // Performs a check if requested 'index' and 'size' are withint the PS boundaries
    bool _indexCheck(ps_address_t index, ps_size_t size) const;

    // Read a byte from EEPROM
    uint8_t _readByte(ps_address_t index) const;

    // Read bytes from EEPROM
    bool _readBytes(ps_address_t index, uint8_t* bytes, ps_size_t size) const;

    // Update a byte to EEPROM
    bool _updateByte(ps_address_t index, uint8_t byte) const;

    // Update bytes to EEPROM
    bool _updateBytes(ps_address_t index, const uint8_t* bytes, ps_size_t size) const;

    // Read the next value
    EmPersistentValueBase* _readNext(ps_address_t& index) const;

    // The first persistent value address
    ps_address_t _firstPvAddress() const;
    
private:
    ps_address_t m_BeginIndex;
    ps_address_t m_EndIndex;
    ps_address_t m_NextPvAddress;
};

/***
    A unique ID assigned to a persistent value.
    The ID MUST be not longer than 'c_MaxLen' chars!
***/    
class EmPersistentId {
    friend class EmPersistentState;
    friend class EmPersistentValueBase;
public:
    const static uint8_t c_MaxLen = 3;

    EmPersistentId(char a, char b=0, char c=0) {
        uint8_t i = 0;
        m_Id[i++] = a;
        m_Id[i++] = b;
        m_Id[i++] = c;
        m_Id[i] = 0;
    }

    EmPersistentId(const char* id) {
        if (strlen(id) > c_MaxLen) {
            // ID longer that c_MaxLen chars!
            m_Id[c_MaxLen] = 0;
        }
        for(uint8_t i=0; i < c_MaxLen; i++) {
            m_Id[i] = strlen(id) > i ? id[i] : 0;
        }
        m_Id[c_MaxLen] = 0;
    }

    EmPersistentId(const EmPersistentId& id) {
        memcpy(m_Id, id.m_Id, sizeof(m_Id));
    }

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentId() {
    }

    char operator[](const int index ) const { 
        if (index >= c_MaxLen) {
            // index out of range!
            return 0;
        }
        return m_Id[index]; 
    }
    
    bool operator==(const EmPersistentId& id) const { 
        return 0 == memcmp(m_Id, id.m_Id, sizeof(m_Id)); 
    }

    bool operator!=(const EmPersistentId& id) const { 
        return !(*this == id);
    }

    operator const char*() const { 
        return GetId();
    }

    const char* GetId() const {
        return m_Id;
    }
    
protected:
    EmPersistentId() {
        memset(m_Id, 0, sizeof(m_Id));
    }

    // Read this object from PS
    bool _read(const EmPersistentState& ps, ps_address_t index);

    // Store this object to PS
    bool _store(const EmPersistentState& ps, ps_address_t index) const;

private:
    char m_Id[c_MaxLen+1];
};

/***
    The base persistent value stored in persistent state (without template defs!)
***/
class EmPersistentValueBase {
    friend class EmPersistentState;
    friend class EmPersistentValueIterator;
public:    
    virtual ~EmPersistentValueBase() {
        if (NULL != m_pValue) {
            free(m_pValue);
        }
    }

    const EmPersistentId& Id() const {
        return m_Id;
    }

    ps_address_t Address() const {
        return m_Address;
    }

    ps_size_t Size() const {
        return m_BufferSize;
    }

    bool IsStored() const {
        return 0 != m_Address;
    }

    // Checks if this two persistent values matches (i.e. have same Id and same Size)
    bool Match(const EmPersistentValueBase& pv) const { 
        return Id() == pv.Id() && Size() == pv.Size(); 
    }

protected:
    EmPersistentValueBase(const EmPersistentState& ps,
                          const char* id,
                          ps_address_t address,
                          ps_size_t bufferSize,
                          void* pInitValue = NULL);

    ps_address_t _idAddress() const {
        return m_Address;
    }

    ps_address_t _sizeAddress() const {
        return m_Address+EmPersistentId::c_MaxLen;
    }

    ps_address_t _valueAddress() const {
        return m_Address+EmPersistentId::c_MaxLen+sizeof(ps_size_t);
    }

    ps_address_t _nextPvAddress() const {
        return m_Address+EmPersistentId::c_MaxLen+sizeof(ps_size_t)+m_BufferSize;
    }

    // Update the value to PS
    bool _updateValue() const {
        return m_Ps._updateBytes(_valueAddress(), (const uint8_t*)m_pValue, m_BufferSize);
    }

    void _setValue(void* pValue) {
        memcpy(m_pValue, pValue, m_BufferSize);
    }

    bool _store() const;

    void _copyFrom(EmPersistentValueBase* pPv) {
        memcpy(m_pValue, pPv->m_pValue, m_BufferSize);
        m_Address = pPv->m_Address;
    }

protected:
    const EmPersistentState& m_Ps;
    const EmPersistentId m_Id;
    ps_address_t m_Address;
    ps_size_t m_BufferSize;
    void* m_pValue;
};

inline bool _itemsMatch(const EmPersistentValueBase& pv1, 
                        const EmPersistentValueBase& pv2) { 
    // This method is used to see if two items are the same in terms of EEPROM storage
    return pv1.Match(pv2); 
}

/***
    The user definable persistent value having templated type
***/
template<class T>
class EmPersistentValue: public EmPersistentValueBase {
    friend class EmPersistentState;
public:    
    EmPersistentValue(const EmPersistentState& ps, 
                      const char* id,
                      T initValue)
    : EmPersistentValueBase(ps, 
                            id, 
                            0,
                            sizeof(T)) {
        SetValue(initValue);
    }

    virtual bool GetValue(T& value) const {
        memcpy(&value, m_pValue, m_BufferSize);
        return true;
    }

    virtual bool SetValue(const T& value) {
        memcpy(m_pValue, &value, m_BufferSize);
        return _updateValue();
    }

    virtual bool operator==(const T& other) const { 
        return other == (T)*this; 
    }

    virtual bool operator!=(const T& other) const { 
        return other != (T)*this; 
    }

    virtual operator T() const { 
        T v = 0;
        GetValue(v);
        return v; 
    }

    virtual operator void*() const { 
        return (void*)m_pValue; 
    }

    virtual T operator =(T value) { 
        SetValue(value);
        return value; 
    }

protected:
    EmPersistentValue(const EmPersistentState& ps,
                      const char* id,
                      ps_address_t address,
                      ps_size_t size,
                      void* pValue)
     : EmPersistentValueBase(ps, id, address, size, pValue) {}
};

// Common value types
typedef EmPersistentValue<int8_t> EmPersistentInt8;
typedef EmPersistentValue<uint8_t> EmPersistentUInt8;
typedef EmPersistentValue<int16_t> EmPersistentInt16;
typedef EmPersistentValue<uint16_t> EmPersistentUInt16;
typedef EmPersistentValue<int32_t> EmPersistentInt32;
typedef EmPersistentValue<uint32_t> EmPersistentUInt32;
typedef EmPersistentValue<int64_t> EmPersistentInt64;
typedef EmPersistentValue<uint64_t> EmPersistentUInt64;
typedef EmPersistentValue<float> EmPersistentFloat;
typedef EmPersistentValue<double> EmPersistentDouble;

class EmPersistentString: public EmPersistentValue<char*> {
public:
    EmPersistentString(const EmPersistentState& ps,
                       const char* id,
                       ps_size_t maxTextLen,
                       const char* initValue)
    : EmPersistentValue(ps, id, (ps_address_t)0, maxTextLen+1, NULL) {
        // NOTE:
        //   We NEED to copy initValue within this constructor and NOT base one!
        memcpy(m_pValue, initValue, _valueSize(initValue));
    }
    
    virtual bool GetValue(char* value) const {
        memcpy(value, m_pValue, _valueSize(value));
        return true;
    }

    virtual bool SetValue(const char* value) {
        memcpy(m_pValue, value, _valueSize(value));
        return _updateValue();
    }

    virtual operator const char*() const { 
        return (const char*)m_pValue; 
    }

    virtual char* operator =(const char* value) {         
        SetValue(value);
        return (char*)m_pValue;
    }

protected:
    virtual void _setValue(void* pValue) {
        memcpy(m_pValue, pValue, _valueSize((const char*)pValue));
    }

    virtual ps_size_t _valueSize(const char* pValue) const {
        if (NULL == pValue) {
            return 0;
        }
        // +1 -> Need to set the string terminator as well
        size_t valueSize = strlen(pValue)+1; 
        // -1 -> Need to leave the string terminator (i.e. max length reached!)
        return (size_t)(m_BufferSize-1) < valueSize ? 
               (ps_size_t)(m_BufferSize-1) : (ps_size_t)valueSize;
    }
};

/***
    The persistent values iterator
***/
class EmPersistentValueIterator  {
    friend class EmPersistentState;
public:
    EmPersistentValueIterator() 
     : m_pItem(NULL), 
       m_EndReached(false) {}

    ~EmPersistentValueIterator() {
        Reset();
    }

    void Reset() {
        if (NULL != m_pItem) {
            delete m_pItem;
        }
        m_pItem = NULL;
        m_EndReached = false;
    }

    operator EmPersistentValueBase*() {
        return m_pItem;
    }

    EmPersistentValueBase* Item() {
        return m_pItem;
    }

    bool EndReached() {
        return m_EndReached;
    }

protected:

    void _setItem(EmPersistentValueBase* pItem) {
        Reset();
        m_pItem = pItem;        
        m_EndReached = NULL == pItem;
    }

private:
    EmPersistentValueBase* m_pItem;
    bool m_EndReached;
};

