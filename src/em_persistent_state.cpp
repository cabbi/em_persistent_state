#include "em_persistent_state.h"


const EmPersistentId EmPersistentState::c_HeaderId = EmPersistentId("#>!"); 
const EmPersistentId EmPersistentState::c_FooterId = EmPersistentId("#<!");

  //--------------------------------------------------
 // EmPersistentState class implementation   
//--------------------------------------------------
EmPersistentState::EmPersistentState(ps_address_t beginIndex,
                                     ps_address_t endIndex)
  : m_BeginIndex(beginIndex),
    m_EndIndex(endIndex),
    m_NextPvAddress(0) {
    if ((int)m_BeginIndex >= EEPROM.end()) {
        m_BeginIndex = EEPROM.begin();
    }
    if ((int)m_EndIndex > EEPROM.end()) {
        m_EndIndex = EEPROM.end();
    }
    if (c_MinSize < m_EndIndex - m_BeginIndex) {
        // TODO: could improve this by setting only a new begin or a new end
        m_BeginIndex = EEPROM.begin();
        m_EndIndex = EEPROM.end();
    }
}

bool EmPersistentState::Init() {
    // Check initialization
    _init();
    if (!IsInitialized()) {
        return false;
    }
    // Set the next PS address (i.e. the one after the last stored value)
    EmPersistentValueIterator it;
    while (Iterate(it)) { 
        m_NextPvAddress = it.Item()->_nextPvAddress();
    }
    return true;
}

bool EmPersistentState::Init(const EmPersistentValueList& values,
                             bool removeUnusedValues) {
    // Check initialization
    _init();
    if (!IsInitialized()) {
        return false;
    }
    // Assign already stored values
    ps_size_t countItems = 0;
    ps_size_t foundItems = 0;
    EmPersistentValueIterator psIt;
    while (Iterate(psIt)) {
        countItems++;
        EmPersistentValueBase* pv = values.Find(psIt);
        if (NULL != pv) {
            pv->_copyFrom(psIt);
            foundItems++; 
        }
    }
    // Set new values into PS
    EmListIterator<EmPersistentValueBase> valuesIt;
    const bool somethingToDelete = countItems > foundItems;
    if (removeUnusedValues && somethingToDelete) {
        // Write user values from beginning of PS by overwriting old/unused ones
        m_NextPvAddress = _firstPvAddress();
        while (values.Iterate(valuesIt)) {
            _appendValue(valuesIt);
        }        
    } else {
        // Get new values and append them to PS
        while (values.Iterate(valuesIt)) {
            if (!valuesIt.Item()->IsStored()) {
                _appendValue(valuesIt);
            }
        }        
    }
    return true;
}

bool EmPersistentState::Load(EmPersistentValueList& values) {
    // Check initialization
    if (!IsInitialized()) {
        return false;
    }
    
    m_NextPvAddress = _firstPvAddress();
    EmPersistentValueBase* pv = NULL;
    do {
        pv = _readNext(m_NextPvAddress);
        if (NULL != pv) {
            values.Append(pv, true);
        }
    } while (pv != NULL);
    
    return true;
}

bool EmPersistentState::Iterate(EmPersistentValueIterator& iterator) {
    // Check initialization
    if (!IsInitialized()) {
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
    iterator._setItem(_readNext(index));
    return NULL != iterator.Item();
}

bool EmPersistentState::Clear() {
    if (c_HeaderId._store(*this, m_BeginIndex) && 
        c_FooterId._store(*this, _firstPvAddress())) {
        m_NextPvAddress = _firstPvAddress();
        return true;
    }
    return false;
}

bool EmPersistentState::Add(EmPersistentValueBase& value){
    // Check if value is already stored in PS
    EmPersistentValueIterator it;
    while (Iterate(it)) {
        if (value.Match(*it)) {
            // Value already in PS, copy it
            value._copyFrom(it);
            return true; 
        }
    }
    // New value!
    return _appendValue(&value);
}

EmPersistentValueBase* EmPersistentState::_readNext(ps_address_t& index) const {
    // Create a new id object that might be part of new persistent value 
    // (i.e. avoid creating a copy in EmPersistentValueBase)
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
    index += EmPersistentId::c_MaxLen;
    ps_size_t size;
    if (!_readBytes(index, (uint8_t*)&size, sizeof(size))) {
        // Read size failed
        return NULL;
    }
    
    EmPersistentValueBase* pPv = NULL; 
    index += sizeof(size);
    void* pValue = malloc(size);
    if (_readBytes(index, (uint8_t*)pValue, size)) {
        // Read value succeeded: create new persistent value object
        pPv = new EmPersistentValueBase(*this, id.m_Id, address, size, pValue);
        index = pPv->_nextPvAddress();
    } else {
        // Read value failed: free allocated resources
        free(pValue);    
    }
    return pPv;
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

bool EmPersistentState::_init() {
    // Reset the last addresses to none (i.e. list not initialized)
    m_NextPvAddress = 0;
    
    // Find start header
    EmPersistentId id = EmPersistentId();
    if (!id._read(*this, m_BeginIndex)) {        
        return false;
    }
    // Already initialized?
    if (id != c_HeaderId) {
        // Write the PS header
        if (!c_HeaderId._store(*this, m_BeginIndex)) {
            return false;
        }
        // Write the PS footer
        if (!c_FooterId._store(*this, _firstPvAddress())) {
            return false;
        }
    }
    // NOTE: this address will be set to the end of PS by the 'Init'!
    m_NextPvAddress = _firstPvAddress();
    return true;
}

bool EmPersistentState::_indexCheck(ps_address_t index, ps_size_t size) const {
    return index >= m_BeginIndex && (index+size) < m_EndIndex;
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
    return m_BeginIndex + EmPersistentId::c_MaxLen;
}

  //--------------------------------------------------
 // EmPersistentId class implementation   
//--------------------------------------------------
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
