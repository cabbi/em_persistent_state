#include "em_persistent_state.h"


const EmPersistentId EmPersistentState::c_HeaderId = EmPersistentId("#>!"); 
const EmPersistentId EmPersistentState::c_FooterId = EmPersistentId("#<!");

  //--------------------------------------------------
 // EmPersistentState class implementation   
//--------------------------------------------------
EmPersistentState::EmPersistentState(ps_address_t beginIndex,
                                     ps_address_t endIndex)
  : m_Initialized(false),
    m_BeginIndex(beginIndex),
    m_EndIndex(endIndex),
    m_Values(EmPersistentValueList()) {
    if ((int)m_BeginIndex >= EEPROM.end()) {
        m_BeginIndex = EEPROM.begin();
    }
    if ((int)m_EndIndex > EEPROM.end()) {
        m_EndIndex = EEPROM.end();
    }
    if (c_MinSize < m_EndIndex - m_BeginIndex) {
        m_BeginIndex = EEPROM.begin();
        m_EndIndex = EEPROM.end();
    }
}

EmPersistentValueList *pStoredValues, *pNewValues;

bool EmPersistentState::Init(EmPersistentValueList& values,
                           bool removeUnusedValues) {
    m_Initialized = _init();
    if (!m_Initialized) {
        return false;
    }
    // Initialize helper vars
    EmPersistentValueList newValues;
    EmPersistentValueList storedValues;
    pStoredValues = &storedValues;
    pNewValues = &newValues;

    // Clear current memory list
    m_Values.Clear();

    // Get stored value
    if (!_loadStoredList(storedValues)) {
        return false;
    }

    bool res = true;
    // Set currently values and addresses to user list if already stored
    values.ForEach<void>([](EmPersistentValueBase& item, bool, bool, void*) -> EmIterResult {
        // Requested value in current PS?
        EmPersistentValueBase* pStoredValue = pStoredValues->Find(item);
        if (NULL != pStoredValue) {
            // Requested value is already stored, let's get current value and address
            item._setValue(pStoredValue->m_pValue);
            item.m_Address = pStoredValue->m_Address;
            pStoredValues->Remove(pStoredValue);
        } else {
            // Requested value is new one
            pNewValues->Append(item);
        }
        return EmIterResult::moveNext;
    });        

    // Any stored value that is left? (i.e. not more used!)
    if (removeUnusedValues && storedValues.IsNotEmpty()) {
        // Add all user values to storage (storage has been reset before with 'm_Values.Clear()')
        res = values.ForEach<EmPersistentState>([](EmPersistentValueBase& item, bool, bool, EmPersistentState* pThis) -> EmIterResult {
            if (!pThis->_appendValue(item)) {
                return EmIterResult::stopFailed;
            }
            return EmIterResult::moveNext;
        }, this);
    } else {
        // Load current storage
        if (!_loadStoredList(m_Values)) {
            return false;
        }   
        // Add new values to storage
        res = newValues.ForEach<EmPersistentState>([](EmPersistentValueBase& item, bool, bool, EmPersistentState* pThis) -> EmIterResult {
            if (!pThis->_appendValue(item)) {
                return EmIterResult::stopFailed;
            }
            return EmIterResult::moveNext;
        }, this);
    }
    return res;
}

bool EmPersistentState::_loadStoredList(EmPersistentValueList& values) {
    if (!m_Initialized) {
        return false;
    }
    
    ps_address_t index = m_BeginIndex + EmPersistentId::c_MaxLen; // PS has the "header id" at the beginning
    EmPersistentValueBase* pv = NULL;
    do {
        pv = _readNext(index);
        if (NULL != pv) {
            values.Append(pv, true);
        }
    } while (pv != NULL);
    
    return true;
}

EmPersistentValueBase* EmPersistentState::_readNext(ps_address_t& index) const {
    if (!m_Initialized) {
        return NULL;
    }

    EmPersistentId id = EmPersistentId();
    const ps_address_t address = index;
    if (!id._read(*this, index)) {
        return NULL;
    }
    // PS termination?
    if (id == c_FooterId) {
        return NULL;
    }
    // Read the value size
    index += EmPersistentId::c_MaxLen;
    ps_size_t size;
    if (!_readBytes(index, (uint8_t*)&size, sizeof(size))) {
        return NULL;
    }
    
    EmPersistentValueBase* pv = NULL; 
    index += sizeof(size);
    void* pValue = malloc(size);
    if (_readBytes(index, (uint8_t*)pValue, size)) {
        pv = new EmPersistentValueBase(*this, id, address, size, pValue);
        index = pv->_nextPvAddress();
    }
    free(pValue);    
    return pv;
}

bool EmPersistentState::_appendValue(EmPersistentValueBase& value) {
    // Get last item in storage
    EmPersistentValueBase* pLastItem = m_Values.Last();
    if (NULL != pLastItem) {
        value.m_Address = pLastItem->_nextPvAddress();
    } else {
        // PS first address (i.e. begin + header)
        value.m_Address = m_BeginIndex+EmPersistentId::c_MaxLen;
    }
    // Store value into storage and update footer
    if (value._store() && 
        c_FooterId._store(*this, value._nextPvAddress())) {
            m_Values.Append(value);
            return true;
        }
    return false;
}

bool EmPersistentState::_init() {
    // Find start header
    EmPersistentId id = EmPersistentId();
    if (!_readBytes(m_BeginIndex, 
                    (uint8_t*)id.m_Id, 
                    EmPersistentId::c_MaxLen)) {        
        return false;
    }
    // Already initialized?
    if (id != c_HeaderId) {
        // Write the PS header
        if (!_updateBytes(m_BeginIndex, 
                          (uint8_t*)c_HeaderId.m_Id, 
                          EmPersistentId::c_MaxLen)) {
            return false;
        }
        // Write the PS footer
        if (!_updateBytes(m_BeginIndex+EmPersistentId::c_MaxLen, 
                          (uint8_t*)c_FooterId.m_Id, 
                          EmPersistentId::c_MaxLen)) {
            return false;
        }
    }
    // PS initialized
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
                        const EmPersistentId& id,
                        ps_address_t address,
                        ps_size_t bufferSize,
                        const void* pInitValue) 
 : m_Ps(ps),
   m_pId(new EmPersistentId(id)),
   m_Address(address),
   m_BufferSize(bufferSize),
   m_pValue(malloc(bufferSize))
{ 
    if (NULL != m_pValue) {
        // Any initial value to set?
        if (NULL != pInitValue) {
            memcpy(m_pValue, pInitValue, bufferSize);
        } else {
            // Reset current value
            memset(m_pValue, 0, m_BufferSize);
        }
    }
}

bool EmPersistentValueBase::_store() const
{
    // Write the ID
    if (!m_pId->_store(m_Ps, _idAddress())) {
        return false;
    }
    // Write the size
    if (!m_Ps._updateBytes(_sizeAddress(), (const uint8_t*)&m_BufferSize, sizeof(m_BufferSize))) {
        return false;
    }
    // Write the value itself
    return _updateValue();
}
