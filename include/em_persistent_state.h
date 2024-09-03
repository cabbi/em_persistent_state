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

        PersistentState PS;
        PersistentUInt16 intVal = PersistentUInt16(PS, "i_v", 16);
        PersistentFloat floatVal = PersistentFloat(PS, "f_v", 55.3);
        PersistentString textVal = PersistentString(PS, "txt", 10, "Hello!");

        void setup() {
            PersistentValueList values;
            values.Append(floatVal);
            values.Append(intVal);
            values.Append(textVal);
            PS.Init(values, false);
        }

        int main() {
            
            setup();

            // Storing new values to PS
            textVal = *I got new value!"; // This will be truncated because of max len of 10!
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

    virtual ~EmPersistentState() {
        m_Values.Clear();
    }

    // Initialize the persistent state by loading stored list values, setting current values
    // to the requested 'values' and removing no more requested stored values.
    //
    // If 'removeUnusedValues' is set to true the storage will remove values that are not
    // in the desired 'values' list.
    //
    // NOTE:
    //   adding values to the 'values' list after the 'Init' call has no effect.
    //   (i.e. elements added after this call are not stored into persistent state)
    bool Init(EmPersistentValueList& values, bool removeUnusedValues);


protected:   
    // Initialize the persistent state for the very first time (i.e. writing header and footer ids)
    bool _init();

    // Load the current EEPROM persistent state values
    bool _loadStoredList(EmPersistentValueList& values);

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

    // Append a new value to storage
    bool _appendValue(EmPersistentValueBase& pValue);

private:
    bool m_Initialized;
    ps_address_t m_BeginIndex;
    ps_address_t m_EndIndex;
    EmPersistentValueList m_Values;
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
        if (i < c_MaxLen) {
            //DPrintln("[PersistentId] please review char constructor!");
        }
        m_Id[i] = 0;
    }

    EmPersistentId(const char* id) {
        if (strlen(id) > c_MaxLen) {
            //DPrintln("[PersistentId] ID longer that 3 chars!");
        }
        for(uint8_t i=0; i < c_MaxLen; i++) {
            m_Id[i] = strlen(id) > i ? id[i] : 0;
        }
        m_Id[c_MaxLen] = 0;
    }

    EmPersistentId(const EmPersistentId& id) {
        memcpy(m_Id, id.m_Id, sizeof(m_Id));
    }

    char operator[](const int index ) const { 
        if (index >= c_MaxLen) {
            //DPrintln("[PersistentId] index out of range!");
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

    bool _read(const EmPersistentState& ps, ps_address_t index);

    // Strore this object to PS
    bool _store(const EmPersistentState& ps, ps_address_t index) const;

private:
    char m_Id[c_MaxLen+1];
};

/***
    The base persistent value stored in persistent state (without template defs!)
***/
class EmPersistentValueBase {
    friend class EmPersistentState;
public:    
    virtual ~EmPersistentValueBase() {
        delete m_pId;
        if (NULL != m_pValue) {
            free(m_pValue);
        }
    }

    virtual const EmPersistentId& Id() const {
        return *m_pId;
    }

    virtual ps_address_t Address() const {
        return m_Address;
    }

    virtual ps_size_t Size() const {
        return m_BufferSize;

    }
protected:
    EmPersistentValueBase(const EmPersistentState& ps,
                        const EmPersistentId& id,
                        ps_address_t address,
                        ps_size_t bufferSize,
                        const void* pInitValue);

    virtual ps_address_t _idAddress() const {
        return m_Address;
    }

    virtual ps_address_t _sizeAddress() const {
        return m_Address+EmPersistentId::c_MaxLen;
    }

    virtual ps_address_t _valueAddress() const {
        return m_Address+EmPersistentId::c_MaxLen+sizeof(ps_size_t);
    }

    virtual ps_address_t _nextPvAddress() const {
        return m_Address+EmPersistentId::c_MaxLen+sizeof(ps_size_t)+m_BufferSize;
    }

    // Update the value to PS
    virtual bool _updateValue() const {
        return m_Ps._updateBytes(_valueAddress(), (const uint8_t*)m_pValue, m_BufferSize);
    }

    virtual void _setValue(void* pValue) {
        memcpy(m_pValue, pValue, m_BufferSize);
    }

    virtual bool _store() const;

protected:
    const EmPersistentState& m_Ps;
    EmPersistentId* m_pId;
    ps_address_t m_Address;
    ps_size_t m_BufferSize;
    void* m_pValue;
};

inline bool _itemsMatch(const EmPersistentValueBase& pv1, 
                 const EmPersistentValueBase& pv2) { 
    // This method is used to see if two items are the same in terms of EEPROM storage
    return pv1.Id() == pv2.Id() && pv1.Size() == pv2.Size(); 
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
    : EmPersistentValue(ps, 
                      EmPersistentId(id), 
                      initValue) {}

    EmPersistentValue(const EmPersistentState& ps, 
                    char id1,
                    char id2,
                    char id3,
                    T initValue)
    : EmPersistentValue(ps, 
                      EmPersistentId(id1, id2, id3), 
                      initValue) {}

    EmPersistentValue(const EmPersistentState& ps, 
                    const EmPersistentId& id,
                    T initValue)
    : EmPersistentValueBase(ps, 
                          id, 
                          0,
                          sizeof(T),
                          &initValue) {}

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
                      const EmPersistentId& id,
                      ps_address_t address,
                      ps_size_t size,
                      const void* pValue)
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
    : EmPersistentValue(ps, EmPersistentId(id), (ps_address_t)0, maxTextLen+1, NULL) {
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