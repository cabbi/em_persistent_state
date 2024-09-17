#include "Arduino.h"
#include "em_persistent_state.h"


const EmPersistentId EmPersistentState::c_HeaderId = EmPersistentId("#>!"); 
const EmPersistentId EmPersistentState::c_FooterId = EmPersistentId("#<!");

  //--------------------------------------------------
 // EmPersistentState class implementation   
//--------------------------------------------------
EmPersistentState::EmPersistentState(EmLogLevel logLevel, 
                                     ps_address_t beginIndex,
                                     ps_address_t endIndex)
  : EmLog("PS", logLevel),
    m_BeginIndex(beginIndex),
    m_EndIndex(endIndex),
    m_NextPvAddress(0) {
    if ((int)m_BeginIndex >= EEPROM.end()) {
        m_BeginIndex = EEPROM.begin();
    }
    if ((int)m_EndIndex > EEPROM.end()) {
        m_EndIndex = EEPROM.end();
    }
    if (c_MinSize > (m_EndIndex - m_BeginIndex)) {
        // TODO: could improve this by setting only a new begin or a new end
        m_BeginIndex = EEPROM.begin();
        m_EndIndex = EEPROM.end();
    }
}

int EmPersistentState::Init() {
    // Reset the last addresses to none (i.e. list not initialized)
    m_NextPvAddress = 0;
    
    // Find start header
    EmPersistentId id;
    if (!id._read(*this, m_BeginIndex)) {  
        LogError(F("Init failed by reading header!"));      
        return -1;
    }
    // Already initialized?
    if (id != c_HeaderId) {
        // Write the PS header
        if (!c_HeaderId._store(*this, m_BeginIndex)) {
            LogError(F("Init failed by storing header!"));      
            return -1;
        }
        // Write the PS footer
        if (!c_FooterId._store(*this, _firstPvAddress())) {
            LogError(F("Init failed by storing footer!"));      
            return -1;
        }
    }
    // Set the next PS address (i.e. the one after the last stored value)
    int count = 0;
    m_NextPvAddress = _firstPvAddress();
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (_readNext(m_NextPvAddress, psId, psSize)) {
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        m_NextPvAddress = (ps_address_t)(m_NextPvAddress + psSize);
        count++;
    }
    LogInfo(F("Init succeeded"));      
    return count;
}

int EmPersistentState::Init(const EmPersistentValueList& values,
                             bool removeUnusedValues) {
    // Check initialization
    const int countItems = Init();
    if (countItems < 0) {
        return countItems;
    }
    EmListIterator<EmPersistentValueBase> it;
    // Assign already stored values
    ps_size_t foundItems = 0;
    while (values.Iterate(it)) {
        if (Find(*it.Item())) {
            foundItems++; 
        }
    }
    // Set new values into PS
    const bool somethingToDelete = countItems > foundItems;
    if (removeUnusedValues && somethingToDelete) {
        // Write user values from beginning of PS by overwriting old/unused ones
        m_NextPvAddress = _firstPvAddress();
        while (values.Iterate(it)) {
            _appendValue(it);
        }        
    } else {
        // Get new values and append them to PS
        while (values.Iterate(it)) {
            if (!it.Item()->IsStored()) {
                _appendValue(it);
            }
        }        
    }
    return countItems;
}

int EmPersistentState::Load(EmPersistentValueList& values) {
    // Check initialization
    if (!_isInitialized(true)) {
        return -1;
    }
    
    int count = 0;
    ps_address_t index = _firstPvAddress();
    EmPersistentValueBase* pv = NULL;
    do {
        pv = _createNext(index);
        if (NULL != pv) {
            values.Append(pv, true);
            count++;
        }
    } while (pv != NULL);
    
    return count;
}

int EmPersistentState::Count() {
    // Check initialization
    if (!_isInitialized(true)) {
        return -1;
    }
    int count = 0;
    // Set the next PS address (i.e. the one after the last stored value)
    ps_address_t index = _firstPvAddress();
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (_readNext(index, psId, psSize)) {
        count++;
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        index = (ps_address_t)(index + psSize);
    }
    return count;
}

bool EmPersistentState::Iterate(EmPersistentValueIterator& iterator) {
    // Check initialization
    if (!_isInitialized(true)) {
        return false;
    }
    // Iterator reached it end?
    if (iterator.EndReached()) {
        return false;
    }
    // Get the next required PV address
    ps_address_t index = 0;
    if (NULL == iterator.Item()) {
        // First address
        index = _firstPvAddress();
    } else {
        // Go to next address
        index = iterator.Item()->_nextPvAddress();
    }
    // Read next item from PS
    iterator._setItem(_createNext(index));
    return NULL != iterator.Item();
}

bool EmPersistentState::Clear() {
    if (c_HeaderId._store(*this, m_BeginIndex) && 
        c_FooterId._store(*this, _firstPvAddress())) {
        m_NextPvAddress = _firstPvAddress();
        return true;
    }
    LogError(F("Clear failed!"));      
    return false;
}

bool EmPersistentState::Add(EmPersistentValueBase& value){
    // Check initialization
    if (!_isInitialized(true)) {
        return false;
    }
    // Check if value is already stored in PS
    if (Find(value)) {
        // Value already in PS
        return true; 
    }
    // Not found, append a new value to PS
    return _appendValue(&value);
}

bool EmPersistentState::Find(EmPersistentValueBase& value){
    // Check initialization
    if (!_isInitialized(true)) {
        return false;
    }
    // Find matching id & size
    ps_address_t index = _firstPvAddress();
    if (!_findMatch(index, value.Id(), value.Size())) {
        return false;
    }
    // Set value PS's address
    value.m_Address = (ps_address_t)(index - EmPersistentId::c_MaxLen - (ps_address_t)sizeof(ps_size_t));
    // Read its value
    if (!_readBytes(index, (uint8_t*)value.m_pValue, value.Size())) {
        return false;
    }
    return true;
}

bool EmPersistentState::_findMatch(ps_address_t& index, 
                                   const EmPersistentId& id,
                                   ps_size_t size) const {
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (!EmPersistentValueBase::_match(id, psId, size, psSize)) {
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        index = (ps_address_t)(index + psSize);
        // Read next PS id & size
        if (!_readNext(index, psId, psSize)) {
            return false;
        }
    }
    // Item found
    return true;
}                                    

bool EmPersistentState::_readNext(ps_address_t& index, 
                                  EmPersistentId& id,
                                  ps_size_t& size) const {
    // Read PS id
    if (!id._read(*this, index)) {
        // Read id failed
        return false;
    }
    // PS termination?
    if (id == c_FooterId) {
        // End of persistent state
        return false;
    }
    // Read PS size
    // NOTE: avoid conversion warning using += operator 
    index = (ps_address_t)(index + EmPersistentId::c_MaxLen);
    if (!_readBytes(index, (uint8_t*)&size, sizeof(size))) {
        // Read size failed
        return false;
    }
    // Move to value index
    index = (ps_address_t)(index + sizeof(size));
    return true;
}

EmPersistentValueBase* EmPersistentState::_createNext(ps_address_t& index) const {
    EmPersistentId id;
    const ps_address_t address = index;
    if (!id._read(*this, index)) {
        // Read id failed
        return NULL;
    }
    // PS termination?
    if (id == c_FooterId) {
        // End of persistent state
        return NULL;
    }
    // Read the value size
    // NOTE: avoid conversion warning using += operator 
    index = (ps_address_t)(index + EmPersistentId::c_MaxLen);
    ps_size_t size;
    if (!_readBytes(index, (uint8_t*)&size, sizeof(size))) {
        // Read size failed
        return NULL;
    }
    
    EmPersistentValueBase* pPv = NULL; 
    // NOTE: avoid conversion warning using += operator 
    index = (ps_address_t)(index + sizeof(size));
    void* pValue = malloc(size);
    if (_readBytes(index, (uint8_t*)pValue, size)) {
        // Read value succeeded: create new persistent value object
        pPv = new EmPersistentValueBase(*this, id.m_Id, address, size, pValue);
        index = (ps_address_t)(index + size);
    } else {
        // Read value failed: free allocated resources
        free(pValue);    
    }
    return pPv;
}

bool EmPersistentState::_isInitialized(bool logError) const {
    if (0 == m_NextPvAddress) {
        if (logError) {
            LogError(F("PS not initialized!"));      
        }
        return false;
    }
    return true;
}

bool EmPersistentState::_appendValue(EmPersistentValueBase* pValue) {
    pValue->m_Address = m_NextPvAddress;
    // Store value into storage and update footer
    if (pValue->_store() && c_FooterId._store(*this, pValue->_nextPvAddress())) {
        m_NextPvAddress = pValue->_nextPvAddress(); 
        return true;
    }
    pValue->m_Address = 0;
    return false;
}

bool EmPersistentState::_indexCheck(ps_address_t index, ps_size_t size) const {    
    bool res = index >= m_BeginIndex && (index+size) < m_EndIndex;
    if (!res) {
        LogError<50>("Index out of range: %d < %d + %d < %d", 
                     m_BeginIndex, index, size, m_EndIndex);
    }
    return res;
}    

uint8_t EmPersistentState::_readByte(ps_address_t index) const {
    if (!_indexCheck(index, 1)) {
        return 0;
    }
    return EEPROM.read(index);
}

bool EmPersistentState::_readBytes(ps_address_t index, uint8_t* bytes, ps_size_t size) const {
    if (!_indexCheck(index, size)) {
        return false;
    }
    for(ps_address_t i=0; i<size; i++) {
        bytes[i] = EEPROM.read(index+i);
    }
    return true;
}

bool EmPersistentState::_updateByte(ps_address_t index, uint8_t byte) const {
    if (!_indexCheck(index, 1)) {
        return false;
    }
    if (byte != EEPROM.read(index)) {
        EEPROM.write(index, byte);
    }
    return true;
}

bool EmPersistentState::_updateBytes(ps_address_t index, const uint8_t* bytes, ps_size_t size) const {
    if (!_indexCheck(index, size)) {
        return false;
    }
    for(ps_address_t i=0; i<size; i++) {
        if (bytes[i] != EEPROM.read(index+i)) {
            EEPROM.write(index+i, bytes[i]);
        }
    }
    return true;
}

inline ps_address_t EmPersistentState::_firstPvAddress() const {
    return (ps_address_t)(m_BeginIndex + EmPersistentId::c_MaxLen);
}

  //--------------------------------------------------
 // EmPersistentId class implementation   
//--------------------------------------------------
EmPersistentId::EmPersistentId(char a, char b, char c) {
    uint8_t i = 0;
    m_Id[i++] = a;
    m_Id[i++] = b;
    m_Id[i++] = c;
    m_Id[i] = 0;
}

EmPersistentId::EmPersistentId(const char* id) {
    if (strlen(id) > c_MaxLen) {
        // ID longer that c_MaxLen chars!
        m_Id[c_MaxLen] = 0;
    }
    for(uint8_t i=0; i < c_MaxLen; i++) {
        m_Id[i] = strlen(id) > i ? id[i] : 0;
    }
    m_Id[c_MaxLen] = 0;
}

EmPersistentId::EmPersistentId(const EmPersistentId& id) {
    memcpy(m_Id, id.m_Id, sizeof(m_Id));
}

char EmPersistentId::operator[](const int index) const
 { 
    if (index >= c_MaxLen) {
        //LogError("index out of range!");
        return 0;
    }
    return m_Id[index]; 
}

bool EmPersistentId::_store(const EmPersistentState& ps, ps_address_t index) const {
    return ps._updateBytes(index, (const uint8_t*)m_Id, c_MaxLen);
}


bool EmPersistentId::_read(const EmPersistentState& ps, ps_address_t index) {
    return ps._readBytes(index, (uint8_t*)m_Id, c_MaxLen);
}

  //--------------------------------------------------
 // EmPersistentValueBase class implementation   
//--------------------------------------------------
EmPersistentValueBase::EmPersistentValueBase(const EmPersistentState& ps,
                                             const char* id,
                                             ps_address_t address,
                                             ps_size_t bufferSize,
                                             void* pInitValue) 
 : m_Ps(ps),
   m_Id(EmPersistentId(id)),
   m_Address(address),
   m_BufferSize(bufferSize),
   m_pValue(pInitValue) {
    if (NULL == m_pValue) {
        m_pValue = malloc(m_BufferSize);
        memset(m_pValue, 0, m_BufferSize);
    }
}

bool EmPersistentValueBase::_store() const
{
    // Write the ID
    if (!m_Id._store(m_Ps, _idAddress())) {
        return false;
    }
    // Write the size
    if (!m_Ps._updateBytes(_sizeAddress(), (const uint8_t*)&m_BufferSize, sizeof(m_BufferSize))) {
        return false;
    }
    // Write the value itself
    return _updateValue();
}
