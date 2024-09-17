#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "EEPROM.h"

#include "em_log.h"
#include "em_defs.h"
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
            //       spare memory by not filling an 'EmPersistentValueList' 
            if (PS.Init() >= 0) {
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
class EmPersistentState: public EmLog {
    friend class EmPersistentValueBase;
    friend class EmPersistentId;
public:    
    const static EmPersistentId c_HeaderId; 
    const static EmPersistentId c_FooterId;
    const static int c_MinSize = 12;

    EmPersistentState(EmLogLevel logLevel = EmLogLevel::none,
                      ps_address_t beginIndex = EEPROM.begin(),
                      ps_address_t endIndex = EEPROM.end());

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentState() {
    }

    // Initialize the persistent state without changing current stored values.
    //
    // Return the persistent state stored values count or -1 if persistent state 
    // has not been successfully initialized.
    int Init();

    // Initialize the persistent state by loading 'values' list, 
    // setting current values to the requested 'values'.
    //
    // Return the persistent state stored values count or -1 if persistent state 
    // has not been successfully initialized.
    //
    // If 'removeUnusedValues' is set to true the storage will remove 
    // values that are not in the desired 'values' list.
    //
    // NOTE:
    //   appending values to the 'values' list after the 'Init' call has no effect.
    //   Use the persistent state 'Add' method to add values to it.
    int Init(const EmPersistentValueList& values, bool removeUnusedValues);

    // Checks if persistent state has been initialized (i.e. 'Init' call!)
    bool IsInitialized() const {
        return _isInitialized(false);
    }

    // Load the current persistent values into a list
    // Return the number of loaded values or -1 if persistent state has not been initialized.
    // NOTE:
    //  This method is dynamically allocating heap memory.
    int Load(EmPersistentValueList& values);

    // Iterate the persistent values one by one without generating a full elements list
    // NOTE:
    //  This method is dynamically allocating and deallocating heap memory.
    bool Iterate(EmPersistentValueIterator& iterator);

    // Add a value to storage. 
    // This method will check if 'value' is already stored and set its 
    // current value taken from persistent state.
    bool Add(EmPersistentValueBase& value);

    // Find this 'value' identified by its Id and Size.
    // If found the persistent state stored value is set to 'value'.
    // Return true if value has been fond in PS.
    bool Find(EmPersistentValueBase& value);

    // Count the persistent state stored values or -1 if persistent state 
    // has not been initialized.
    // NOTE:
    //  This method is iterating trough all persistent state stored values.
    int Count();

    // Clear the PS by resetting all its stored values
    bool Clear();

protected:   

    // Checks if persistent state has been initialized
    bool _isInitialized(bool logError) const;

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
    bool _updateBytes(ps_address_t index, 
                      const uint8_t* bytes, 
                      ps_size_t size) const;

    // Find the matching id & size
    bool _findMatch(ps_address_t& index, 
                    const EmPersistentId& id, 
                    ps_size_t size) const;
                                   
    // Read the next PS id and size
    bool _readNext(ps_address_t& index, 
                   EmPersistentId& id,
                   ps_size_t& size) const;

    // Create a new persistent value reading the next PS item
    EmPersistentValueBase* _createNext(ps_address_t& index) const;

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

    EmPersistentId(char a, char b=0, char c=0);
    EmPersistentId(const char* id);
    EmPersistentId(const EmPersistentId& id);

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentId() {
    }

    char operator[](const int index ) const;
    
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
        return _match(Id(), pv.Id(), Size(), pv.Size());
    }

protected:

    static bool _match(const EmPersistentId& id1,
                       const EmPersistentId& id2,
                       ps_size_t size1,
                       ps_size_t size2) { 
        return id1 == id2 && size1 == size2; 
    }

    EmPersistentValueBase(const EmPersistentState& ps,
                          const char* id,
                          ps_address_t address,
                          ps_size_t bufferSize,
                          void* pInitValue = NULL);

    ps_address_t _idAddress() const {
        return m_Address;
    }

    ps_address_t _sizeAddress() const {
        return (ps_address_t)(m_Address+EmPersistentId::c_MaxLen);
    }

    ps_address_t _valueAddress() const {
        return (ps_address_t)(m_Address+EmPersistentId::c_MaxLen+
                             (ps_address_t)sizeof(ps_size_t));
    }

    ps_address_t _nextPvAddress() const {
        return (ps_address_t)(m_Address+EmPersistentId::c_MaxLen+
                              (ps_address_t)sizeof(ps_size_t)+m_BufferSize);
    }

    // Update the value to PS
    bool _updateValue() const {
        return m_Ps._updateBytes(_valueAddress(), (const uint8_t*)m_pValue, m_BufferSize);
    }

    virtual void _getMem(void* pValue) const {
        memcpy(pValue, m_pValue, m_BufferSize);
    }

    virtual void _setMem(const void* pValue) {
        memcpy(m_pValue, pValue, m_BufferSize);
    }

    virtual bool _store() const;

    virtual void _copyFrom(EmPersistentValueBase* pPv) {
        memcpy(m_pValue, pPv->m_pValue, m_BufferSize);
        m_Address = pPv->m_Address;
    }

protected:
    const EmPersistentState& m_Ps;
    EmPersistentId m_Id;
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
        // NOTE: 
        //  set memory directly instead calling 'SetValue' since Address is not set!
        memcpy(m_pValue, &initValue, m_BufferSize);
    }

    virtual bool GetValue(T& value) const {
        _getMem(&value);
        return true;
    }

    virtual bool SetValue(const T& value) {
        // Avoid writing same value to EEPROM (only time consuming!)
        if (0 == memcmp(m_pValue, &value, m_BufferSize)) {
            return true;
        }
        _setMem(&value);
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
    : EmPersistentValue(ps, id, (ps_address_t)0, (ps_size_t)(maxTextLen+1), NULL) {
        // NOTE:
        //   We NEED to copy initValue within this constructor and NOT base one!
        memcpy(m_pValue, initValue, _valueSize(initValue));
    }
    
    virtual bool GetValue(char* value) const {
        _getMem((void*)value);
        return true;
    }

    virtual bool SetValue(const char* value) {
        // Avoid writing same value to EEPROM (only time consuming!)
        if (0 == memcmp(m_pValue, &value, _valueSize(value))) {
            return true;
        }
        _setMem(value);
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
    virtual void _getMem(void* pValue) const {
        memcpy(pValue, m_pValue, _valueSize((const char*)pValue));
    }

    virtual void _setMem(const void* pValue) {
        memcpy(m_pValue, pValue, _valueSize((const char*)pValue));
    }

    virtual ps_size_t _valueSize(const char* pValue) const {
        if (NULL == pValue) {
            return 0;
        }
        // +1 -> Need to set the string terminator as well
        ps_size_t valueSize = (ps_size_t)(strlen(pValue)+1); 
        // -1 -> Need to leave the string terminator (i.e. max length reached!)
        return (ps_size_t)MIN(m_BufferSize-1, valueSize);
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
