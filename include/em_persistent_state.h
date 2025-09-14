#ifndef _EM_PERSISTENT_STATE_H_
#define _EM_PERSISTENT_STATE_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "EEPROM.h"

#include "em_log.h"
#include "em_defs.h"
#include "em_optional.h"
#include "em_list.h"
#include "em_sync_value.h"
#include "em_tag.h"

// Persistent State types definition
typedef uint16_t ps_size_t;
typedef uint16_t ps_address_t;

// Forward declaration
class EmPersistentId;
class EmPersistentState;
class EmPersistentValueBase;
class EmPersistentValueIterator;
typedef EmIterator<EmPersistentValueBase> EmPSIterator;
bool itemsMatch_(const EmPersistentValueBase& pv1, 
                 const EmPersistentValueBase& pv2);

/***
    The persistent value list
***/
class EmPersistentValueList: public EmList<EmPersistentValueBase> {
public:
    EmPersistentValueList() : EmList<EmPersistentValueBase>(itemsMatch_) {}
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
            values.append(floatVal);
            values.append(intVal);
            values.append(textVal);
            PS.begin(values, false);
        }

        void setup_() {
            // Second way to initialize Persistent State
            // NOTE: this way we cannot delete old unused values but we 
            //       spare memory by not filling an 'EmPersistentValueList' 
            if (PS.begin() >= 0) {
                PS.add(floatVal);
                PS.add(intVal);
                PS.add(textVal);
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
    friend class EmPersistentId;
    friend class EmPersistentValueBase;
    friend class EmPersistentValueIterator;
public:    
    const static EmPersistentId c_HeaderId; 
    const static EmPersistentId c_FooterId;
    const static int c_MinSize = 12;

#ifdef EM_EEPROM
    EmPersistentState(EmOptional<ps_address_t> beginIndex = emUndefined,
                      EmOptional<ps_address_t> endIndex = emUndefined,
                      EmLogLevel logLevel = EmLogLevel::global);
#else
    EmPersistentState(ps_size_t size = 1024,
                      EmLogLevel logLevel = EmLogLevel::global);
#endif

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentState() {
    }

    // Initialize the persistent state without changing current stored values.
    //
    // Return the persistent state stored values count or -1 if persistent state 
    // has not been successfully initialized.
    int begin();

    // Initialize the persistent state by loading 'values' iterator, 
    // setting current values to the requested 'values'.
    //
    // Return the persistent state stored values count or -1 if persistent state 
    // has not been successfully initialized.
    //
    // If 'removeUnusedValues' is set to true the storage will remove 
    // values that are not in the desired 'values' iterator.
    //
    // NOTE:
    //   appending values to the 'values' list after the 'Init' call has no effect.
    //   Use the persistent state 'Add' method to add values to it.
    int begin(EmPersistentValueList& values, bool removeUnusedValues);
    int begin(EmPersistentValueBase* values[], size_t count, bool removeUnusedValues);
    int begin(EmPSIterator& pIt, bool removeUnusedValues);

    // Checks if persistent state has been initialized (i.e. 'Init' call!)
    bool isInitialized() const {
        return isInitialized_(false);
    }

    // Load the current persistent values into a list
    // Return the number of loaded values or -1 if persistent state has not been initialized.
    // NOTE:
    //  This method is dynamically allocating heap memory.
    int load(EmPersistentValueList& values);

    // Add a value to storage. 
    // This method will check if 'value' is already stored and set its 
    // current value taken from persistent state.
    bool add(EmPersistentValueBase& value);

    // Find this 'value' identified by its Id and Size.
    // If found the persistent state stored value is set to 'value'.
    // Return true if value has been fond in PS.
    bool find(EmPersistentValueBase& value);

    // Count the persistent state stored values or -1 if persistent state 
    // has not been initialized.
    // NOTE:
    //  This method is iterating trough all persistent state stored values.
    int count();

    // Clear the PS by resetting all its stored values
    bool clear();

protected:   
    // Checks if persistent state has been initialized
    bool isInitialized_(bool logError) const;

    // Append a new value to storage
    bool appendValue_(EmPersistentValueBase* pValue);

    // Performs a check if requested 'index' and 'size' are withint the PS boundaries
    bool indexCheck_(ps_address_t index, ps_size_t size) const;

    // Read a byte from EEPROM
    uint8_t readByte_(ps_address_t index) const;

    // Read bytes from EEPROM
    bool readBytes_(ps_address_t index, uint8_t* bytes, ps_size_t size) const;

    // Update a byte to EEPROM
    bool updateByte_(ps_address_t index, uint8_t byte) const;

    // Update bytes to EEPROM
    bool updateBytes_(ps_address_t index, 
                      const uint8_t* bytes, 
                      ps_size_t size) const;

    // Find the matching id & size
    bool findMatch_(ps_address_t& index, 
                    const EmPersistentId& id, 
                    ps_size_t size) const;
                                   
    // Read the next PS id and size
    bool readNext_(ps_address_t& index, 
                   EmPersistentId& id,
                   ps_size_t& size) const;

    // Create a new persistent value reading the next PS item
    EmPersistentValueBase* createNext_(ps_address_t& index) const;

    // The first persistent value address
    ps_address_t firstPvAddress_() const;

    // Get the next persistent value address
    ps_address_t nextPvAddress_(ps_address_t index) const;

private:
    ps_address_t m_beginIndex;
    ps_address_t m_endIndex;
    ps_address_t m_nextPvAddress;
};

/***
    The persistent value iterator used to iterate PS values
***/
class EmPersistentValueIterator: public EmPSIterator {
public:
    EmPersistentValueIterator(EmPersistentState& ps)
      : m_ps(ps), m_pCurrentPv(nullptr) {}


    virtual ~EmPersistentValueIterator() {
        reset();
    }

    // No copy constructor or assignment operator
    EmPersistentValueIterator(const EmPersistentValueIterator&) = delete;
    EmPersistentValueIterator& operator=(const EmPersistentValueIterator&) = delete;

    virtual void reset() override;
    virtual bool next(EmPersistentValueBase*& pItem) override;

protected:
    EmPersistentState& m_ps;
    EmPersistentValueBase* m_pCurrentPv;
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

    // No assignment operator
    EmPersistentId& operator=(const EmPersistentId&) = delete;

    // NOTE: keep destructor and class without virtual functions to avoid extra RAM consumption
    ~EmPersistentId() {
    }

    char operator[](const int index ) const;
    
    bool operator==(const EmPersistentId& id) const { 
        return 0 == memcmp(m_id, id.m_id, sizeof(m_id)); 
    }

    bool operator!=(const EmPersistentId& id) const { 
        return !(*this == id);
    }

    operator const char*() const { 
        return getId();
    }

    const char* getId() const {
        return m_id;
    }
    
protected:
    EmPersistentId() {
        memset(m_id, 0, sizeof(m_id));
    }

    // Read this object from PS
    bool read_(const EmPersistentState& ps, ps_address_t index);

    // Store this object to PS
    bool store_(const EmPersistentState& ps, ps_address_t index) const;

private:
    char m_id[c_MaxLen+1];
};

/***
    The base persistent value stored in persistent state (without template defs!)
***/
class EmPersistentValueBase {
    friend class EmPersistentState;
    friend class EmPersistentValueIterator;
public:    
    virtual ~EmPersistentValueBase() {
        if (m_pValue != nullptr) {
            free(m_pValue);
        }
    }

    // No copy constructor or assignment operator
    EmPersistentValueBase(const EmPersistentValueBase&) = delete;
    EmPersistentValueBase& operator=(const EmPersistentValueBase&) = delete;

    const EmPersistentId& id() const {
        return m_id;
    }

    ps_address_t address() const {
        return m_address;
    }

    ps_size_t size() const {
        return m_bufferSize;
    }

    bool isStored() const {
        return 0 != m_address;
    }

    // Checks if this two persistent values matches (i.e. have same Id and same Size)
    bool match(const EmPersistentValueBase& pv) const { 
        return match_(id(), pv.id(), size(), pv.size());
    }

protected:

    static bool match_(const EmPersistentId& id1,
                       const EmPersistentId& id2,
                       ps_size_t size1,
                       ps_size_t size2) { 
        return id1 == id2 && size1 == size2; 
    }

    EmPersistentValueBase(const EmPersistentState& ps,
                          const char* id,
                          ps_address_t address,
                          ps_size_t bufferSize,
                          void* pInitValue = nullptr);

    ps_address_t idAddress_() const {
        return m_address;
    }

    ps_address_t sizeAddress_() const {
        return (ps_address_t)(m_address+EmPersistentId::c_MaxLen);
    }

    ps_address_t valueAddress_() const {
        return (ps_address_t)(m_address+EmPersistentId::c_MaxLen+
                             (ps_address_t)sizeof(ps_size_t));
    }

    ps_address_t nextPvAddress_() const {
        return (ps_address_t)(m_address+EmPersistentId::c_MaxLen+
                              (ps_address_t)sizeof(ps_size_t)+m_bufferSize);
    }

    // Update the value to PS
    bool updateValue_() const {
        // Is this value stored (i.e. has been added to PS)?
        if (!isStored()) {
            return false;
        }
        return m_ps.updateBytes_(valueAddress_(), (const uint8_t*)m_pValue, m_bufferSize);
    }

    virtual EmGetValueResult getMem_(void* pValue) const {
        EmGetValueResult res = 0 == memcmp(pValue, m_pValue, m_bufferSize) ?
                               EmGetValueResult::succeedEqualValue :
                               EmGetValueResult::succeedNotEqualValue;
        memcpy(pValue, m_pValue, m_bufferSize);
        return res;
    }

    virtual void setMem_(const void* pValue) {
        memcpy(m_pValue, pValue, m_bufferSize);
    }

    virtual bool store_() const;

    virtual void copyFrom_(EmPersistentValueBase* pPv) {
        memcpy(m_pValue, pPv->m_pValue, m_bufferSize);
        m_address = pPv->m_address;
    }

protected:
    const EmPersistentState& m_ps;
    EmPersistentId m_id;
    ps_address_t m_address;
    ps_size_t m_bufferSize;
    void* m_pValue;
};

inline bool itemsMatch_(const EmPersistentValueBase& pv1, 
                        const EmPersistentValueBase& pv2) { 
    // This method is used to see if two items are the same in terms of EEPROM storage
    return pv1.match(pv2); 
}

/***
    The user definable persistent value having templated type
***/
template<class T>
class EmPersistentValue: public EmPersistentValueBase, public EmValue<T> {
    friend class EmPersistentState;
public:    
    EmPersistentValue(const EmPersistentState& ps, 
                      const char* id,
                      T initValue)
    : EmPersistentValueBase(ps, id, 0, sizeof(T)) {
        // NOTE: 
        //  set memory directly instead calling 'SetValue' since Address is not set!
        memcpy(m_pValue, &initValue, m_bufferSize);
    }

    virtual ~EmPersistentValue() = default;

    // No copy constructor or assignment operator
    EmPersistentValue(const EmPersistentValue&) = delete;
    EmPersistentValue& operator=(const EmPersistentValue&) = delete;

    virtual T getValue() const {
        T v = T();
        getValue(v);
        return v; 
    }

    virtual EmGetValueResult getValue(T& value) const override {
        return getMem_(&value);
    }

    virtual bool setValue(const T& value) override {
        // Avoid writing same value to EEPROM (only time consuming!)
        if (equals(value)) {
            return true;
        }
        setMem_(&value);
        return updateValue_();
    }

    virtual bool equals(const T& value) const {
        return 0 == memcmp(m_pValue, &value, m_bufferSize);
    }

    virtual bool operator==(const T& other) const { 
        return other == this->getValue(); 
    }

    virtual bool operator!=(const T& other) const { 
        return other != this->getValue(); 
    }

    virtual operator T() const { 
        return getValue();
    }

    virtual operator void*() const { 
        return (void*)m_pValue; 
    }

    virtual T operator =(const T& value) { 
        setValue(value);
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
typedef EmPersistentValue<bool> EmPersistentBool;
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

// Persistent Tag class to be used within EmTags
// 
// NOTE: string value type is NOT supported!
class EmPersistentTag: public EmPersistentValue<EmTagValue>, 
                       public EmTagInterface {
    friend class EmPersistentState;
public:
    EmPersistentTag(const EmPersistentState& ps, 
                    const char* id,
                    const EmTagValue& initValue,
                    EmSyncFlags flags)
    : EmPersistentValue<EmTagValue>(ps, id, 0, sizeof(EmTagValueStruct), nullptr),
      EmTagInterface(flags) {
        // NOTE: 
        //  set memory directly instead calling 'SetValue' since Address is not set!
        EmTagValueStruct valueBytes;
        initValue.toStruct(valueBytes);
        memcpy(m_pValue, &valueBytes, m_bufferSize);
    }

    virtual ~EmPersistentTag() = default;

    virtual const char* getId() const override {
        return EmPersistentValue<EmTagValue>::id();
    }

    virtual EmTagValue getValue() const override {
        return EmPersistentValue<EmTagValue>::getValue();
    }

    virtual EmGetValueResult getValue(EmTagValue& value) const override {
        // String value type is NOT supported!
        if (value.getType() == EmTagValueType::vt_string)  {
            return EmGetValueResult::failed;
        }
        EmTagValueStruct valueBytes;
        EmGetValueResult res = getMem_(&valueBytes);
        if (res != EmGetValueResult::failed) {
            value.fromStruct(valueBytes);
        }
        return res;
    }

    virtual bool setValue(const EmTagValue& value) override {
        // String value type is NOT supported!
        if (value.getType() == EmTagValueType::vt_string)  {
            return false;
        }
        // Avoid writing same value to EEPROM (only time consuming!)
        EmTagValueStruct valueBytes;
        value.toStruct(valueBytes);
        if (equals(valueBytes)) {
            return true;
        }
        setMem_(&valueBytes);
        return updateValue_();
    }

    virtual bool equals(const EmTagValue& value) const override {
        EmTagValueStruct valueBytes;
        value.toStruct(valueBytes);
        return equals(valueBytes);
    }

    virtual bool equals(const EmTagValueStruct& value) const {
        return 0 == memcmp(m_pValue, &value, m_bufferSize);
    }
};


class EmPersistentString: public EmPersistentValueBase, public EmValue<char*> {
    friend class EmPersistentState;
public:
    EmPersistentString(const EmPersistentState& ps,
                       const char* id,
                       ps_size_t maxTextLen,
                       const char* initValue)
    : EmPersistentValueBase(ps, id, 0, maxTextLen+1) {
        // NOTE:
        //   We NEED to copy initValue within this constructor and NOT base one!
        memcpy(m_pValue, initValue, valueSize_(initValue));
    }

    virtual const char* getValue() const {
        return (const char*)m_pValue;
    }

    virtual EmGetValueResult getValue(char* value) const {
        return _getMem((void*)value);
    }

    virtual bool setValue(const char* value) {
        // Avoid writing same value to EEPROM (only time consuming!)
        if (equals(value)) {
            return true;
        }
        setMem_(value);
        return updateValue_();
    }

    virtual bool equals(const char* value) const {
        if (value == nullptr) {
            return 0 == ((const char*)m_pValue)[0];
        }
        return 0 == strcmp((const char*)m_pValue, value);
    }

    virtual operator const char*() const { 
        return (const char*)m_pValue; 
    }

    virtual char* operator =(const char* value) {         
        setValue(value);
        return (char*)m_pValue;
    }

    virtual bool operator==(const char* other) const { 
        return this->equals(other); 
    }

    virtual bool operator!=(const char* other) const { 
        return !this->equals(other); 
    }    

protected:
    virtual EmGetValueResult _getMem(void* pValue) const {
        EmGetValueResult res = 0 == memcmp(pValue, m_pValue, valueSize_((const char*)pValue)) ?
                               EmGetValueResult::succeedEqualValue : 
                               EmGetValueResult::succeedNotEqualValue;
        memcpy(pValue, m_pValue, valueSize_((const char*)pValue));
        return res;
    }

    virtual void setMem_(const void* pValue) {
        memcpy(m_pValue, pValue, valueSize_((const char*)pValue));
    }

    virtual ps_size_t valueSize_(const char* pValue) const {
        if (nullptr == pValue) {
            return 0;
        }
        // +1 -> Need to set the string terminator as well
        ps_size_t valueSize = (ps_size_t)(strlen(pValue)+1); 
        // -1 -> Need to leave the string terminator (i.e. max length reached!)
        return (ps_size_t)MIN(m_bufferSize-1, valueSize);
    }
};

#endif // _EM_PERSISTENT_STATE_H_