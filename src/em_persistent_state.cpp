#include <Arduino.h>
#include "em_persistent_state.h"


const EmPersistentId EmPersistentState::c_HeaderId = EmPersistentId("#>!"); 
const EmPersistentId EmPersistentState::c_FooterId = EmPersistentId("#<!");

  //--------------------------------------------------
 // EmPersistentState class implementation   
//--------------------------------------------------
#ifdef  EM_EEPROM
EmPersistentState::EmPersistentState(EmOptional<ps_address_t> beginIndex,
                                     EmOptional<ps_address_t> endIndex,
                                     EmLogLevel logLevel)
  : EmLog("PS", logLevel),
    m_beginIndex(beginIndex.hasValue() ? beginIndex.value() : 0),
    m_endIndex(endIndex.hasValue() ? endIndex.value() : EEPROM.length()),
    m_nextPvAddress(0) {
    if (m_beginIndex >= EEPROM.length()) {
        m_beginIndex = 0x00;
    }
    if (m_endIndex > EEPROM.length()) {
        m_endIndex = EEPROM.length();
    }
    if (c_MinSize > (m_endIndex - m_beginIndex)) {
        // TODO: could improve this by setting only a new begin or a new end
        m_beginIndex = 0x00;
        m_endIndex = c_MinSize;
    }
}
#else    
EmPersistentState::EmPersistentState(ps_size_t size, EmLogLevel logLevel)
  : EmLog("PS", logLevel),
    m_beginIndex(0),
    m_endIndex(size),
    m_nextPvAddress(0) {
    if (c_MinSize > (m_endIndex - m_beginIndex)) {
        m_beginIndex = 0x00;
        m_endIndex = c_MinSize;
    }
}
#endif    


int EmPersistentState::begin() {
#ifndef EM_EEPROM
    // Start the "virtual" EEPROM
    if (EEPROM.begin(m_endIndex) == false) {
        logError(F("EEPROM begin failed!"));      
        return -1;
    }
#endif    
    // Reset the last addresses to none (i.e. list not initialized)
    m_nextPvAddress = 0;
    
    // Find start header
    EmPersistentId id;
    if (!id.read_(*this, m_beginIndex)) {  
        logError(F("Begin failed by reading header!"));      
        return -1;
    }
    // Already initialized?
    if (id != c_HeaderId) {
        // Write the PS header
        if (!c_HeaderId.store_(*this, m_beginIndex)) {
            logError(F("Begin failed by storing header!"));      
            return -1;
        }
        // Write the PS footer
        if (!c_FooterId.store_(*this, firstPvAddress_())) {
            logError(F("Begin failed by storing footer!"));      
            return -1;
        }
    }
    // Set the next PS address (i.e. the one after the last stored value)
    int count = 0;
    m_nextPvAddress = firstPvAddress_();
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (readNext_(m_nextPvAddress, psId, psSize)) {
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        m_nextPvAddress = (ps_address_t)(m_nextPvAddress + psSize);
        count++;
    }
    logInfo(F("Begin succeeded"));      
    return count;
}

int EmPersistentState::begin(EmPersistentValueBase* values[], 
                             size_t count,
                             bool removeUnusedValues) {
    EmArrayIterator<EmPersistentValueBase> it(values, count);
    return begin(it, removeUnusedValues);
}

int EmPersistentState::begin(EmPersistentValueList& values,
                            bool removeUnusedValues) {
    EmListIterator<EmPersistentValueBase> it(values);
    return begin(it, removeUnusedValues);
}

int EmPersistentState::begin(EmPSIterator& it, 
                            bool removeUnusedValues) {
    // Check initialization
    int countItems = begin();
    if (countItems < 0) {
        return countItems;
    }
    // Assign already stored values
    int foundItems = 0;
    EmPersistentValueBase* pItem;
    while (it.next(pItem)) {
        if (find(*pItem)) {
            foundItems++; 
        }
    }   
    // Set new values into PS
    const bool somethingToDelete = countItems > foundItems;
    countItems = 0;
    it.reset();
    if (removeUnusedValues && somethingToDelete) {
        // Write user values from beginning of PS by overwriting old/unused ones
        m_nextPvAddress = firstPvAddress_();
        while (it.next(pItem)) {
            appendValue_(pItem);
            countItems++;
        }        
    } else {
        // Get new values and append them to PS
        while (it.next(pItem)) {
            if (!pItem->isStored()) {
                appendValue_(pItem);
            }
            countItems++;
        }        
    }
    return countItems;
}

int EmPersistentState::load(EmPersistentValueList& values) {
    // Check initialization
    if (!isInitialized_(true)) {
        return -1;
    }
    
    int count = 0;
    ps_address_t index = firstPvAddress_();
    EmPersistentValueBase* pv = nullptr;
    do {
        pv = createNext_(index);
        if (nullptr != pv) {
            values.append(pv, true);
            count++;
        }
    } while (pv != nullptr);
    
    return count;
}

int EmPersistentState::count() {
    // Check initialization
    if (!isInitialized_(true)) {
        return -1;
    }
    int count = 0;
    // Set the next PS address (i.e. the one after the last stored value)
    ps_address_t index = firstPvAddress_();
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (readNext_(index, psId, psSize)) {
        count++;
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        index = (ps_address_t)(index + psSize);
    }
    return count;
}

bool EmPersistentState::clear() {
    if (c_HeaderId.store_(*this, m_beginIndex) && 
        c_FooterId.store_(*this, firstPvAddress_())) {
        m_nextPvAddress = firstPvAddress_();
        return true;
    }
    logError(F("Clear failed!"));      
    return false;
}

bool EmPersistentState::add(EmPersistentValueBase& value){
    // Check initialization
    if (!isInitialized_(true)) {
        return false;
    }
    // Check if value is already stored in PS
    if (find(value)) {
        // Value already in PS
        return true; 
    }
    // Not found, append a new value to PS
    return appendValue_(&value);
}

bool EmPersistentState::find(EmPersistentValueBase& value){
    // Check initialization
    if (!isInitialized_(true)) {
        return false;
    }
    // Find matching id & size
    ps_address_t index = firstPvAddress_();
    if (!findMatch_(index, value.id(), value.size())) {
        return false;
    }
    // Set value PS's address
    value.m_address = (ps_address_t)(index - EmPersistentId::c_MaxLen - (ps_address_t)sizeof(ps_size_t));
    // Read its value
    if (!readBytes_(index, (uint8_t*)value.m_pValue, value.size())) {
        return false;
    }
    return true;
}

bool EmPersistentState::findMatch_(ps_address_t& index, 
                                   const EmPersistentId& id,
                                   ps_size_t size) const {
    EmPersistentId psId;
    ps_size_t psSize = 0;
    while (!EmPersistentValueBase::match_(id, psId, size, psSize)) {
        // Move index to next PS item
        // NOTE: avoid conversion warning using += operator 
        index = (ps_address_t)(index + psSize);
        // Read next PS id & size
        if (!readNext_(index, psId, psSize)) {
            return false;
        }
    }
    // Item found
    return true;
}                                    

bool EmPersistentState::readNext_(ps_address_t& index, 
                                  EmPersistentId& id,
                                  ps_size_t& size) const {
    // Read PS id
    if (!id.read_(*this, index)) {
        // Read id failed
        return false;
    }
    // PS termination?
    if (id == c_FooterId) {
        // End of persistent state
        return false;
    }
    // Read PS size
    ps_address_t nextIndex = nextPvAddress_(index);
    if (!readBytes_(nextIndex, (uint8_t*)&size, sizeof(size))) {
        // Read size failed
        return false;
    }    
    // Move to value index
    index = (ps_address_t)(nextIndex + sizeof(size));
    return true;
}

EmPersistentValueBase* EmPersistentState::createNext_(ps_address_t& index) const {
    EmPersistentId id;
    if (!id.read_(*this, index)) {
        // Read id failed
        return nullptr;
    }
    // PS termination?
    if (id == c_FooterId) {
        // End of persistent state
        return nullptr;
    }
    // Read the value size
    ps_address_t nextIndex = nextPvAddress_(index);
    ps_size_t size;
    if (!readBytes_(nextIndex, (uint8_t*)&size, sizeof(size))) {
        // Read size failed
        return nullptr;
    }

    EmPersistentValueBase* pPv = nullptr;
    // NOTE: avoid conversion warning using += operator
    nextIndex = (ps_address_t)(nextIndex + sizeof(size));
    void* pValue = malloc(size);
    if (readBytes_(nextIndex, (uint8_t*)pValue, size)) {
        // Read value succeeded: create new persistent value object
        pPv = new EmPersistentValueBase(*this, id.m_id, index, size, pValue);
        // Move to next PS item
        index = (ps_address_t)(nextIndex + size);
    } else {
        // Read value failed: free allocated resources
        free(pValue);    
    }
    return pPv;
}

bool EmPersistentState::isInitialized_(bool logError) const {
    if (0 == m_nextPvAddress) {
        if (logError) {
            EmPersistentState::logError(F("PS not initialized!"));      
        }
        return false;
    }
    return true;
}

bool EmPersistentState::appendValue_(EmPersistentValueBase* pValue) {
    pValue->m_address = m_nextPvAddress;
    // Store value into storage and update footer
    if (pValue->store_() && c_FooterId.store_(*this, pValue->nextPvAddress_())) {
        m_nextPvAddress = pValue->nextPvAddress_(); 
        return true;
    }
    pValue->m_address = 0;
    return false;
}

bool EmPersistentState::indexCheck_(ps_address_t index, ps_size_t size) const {    
    bool res = index >= m_beginIndex && (index+size) < m_endIndex;
    if (!res) {
        logError<50>("Index out of range: %d < %d + %d < %d", 
                     m_beginIndex, index, size, m_endIndex);
    }
    return res;
}    

uint8_t EmPersistentState::readByte_(ps_address_t index) const {
    if (!indexCheck_(index, 1)) {
        return 0;
    }
    return EEPROM.read(index);
}

bool EmPersistentState::readBytes_(ps_address_t index, uint8_t* bytes, ps_size_t size) const {
    if (!indexCheck_(index, size)) {
        return false;
    }
    for(ps_address_t i=0; i<size; i++) {
        bytes[i] = EEPROM.read(index+i);
    }
    return true;
}

bool EmPersistentState::updateByte_(ps_address_t index, uint8_t byte) const {
    if (!indexCheck_(index, 1)) {
        return false;
    }
    if (byte != EEPROM.read(index)) {
        EEPROM.write(index, byte);
    }
#ifdef EM_EEPROM
    return true;
#else    
    return EEPROM.commit();
#endif    
}

bool EmPersistentState::updateBytes_(ps_address_t index, const uint8_t* bytes, ps_size_t size) const {
    if (!indexCheck_(index, size)) {
        return false;
    }
    for(ps_address_t i=0; i<size; i++) {
        if (bytes[i] != EEPROM.read(index+i)) {
            EEPROM.write(index+i, bytes[i]);
        }
    }
#ifdef EM_EEPROM
    return true;
#else    
    return EEPROM.commit();
#endif    
}

inline ps_address_t EmPersistentState::firstPvAddress_() const {
    return nextPvAddress_(m_beginIndex);
}

inline ps_address_t EmPersistentState::nextPvAddress_(ps_address_t index) const {
    return (ps_address_t)(index + EmPersistentId::c_MaxLen);
}

  //--------------------------------------------------
 // EmPersistentValueIterator class implementation   
//--------------------------------------------------
bool EmPersistentValueIterator::next(EmPersistentValueBase*& pItem) {
    // Check initialization
    if (!m_ps.isInitialized()) {
        return false;
    }
    // Read next item from PS
    ps_address_t nextAddress = m_pCurrentPv == nullptr ? m_ps.firstPvAddress_() : m_pCurrentPv->nextPvAddress_();
    EmPersistentValueBase* pNextItem = m_ps.createNext_(nextAddress);
    if (pNextItem == nullptr) {
        return false;
    }
    // Free previous item if any
    if (m_pCurrentPv != nullptr) {
        delete m_pCurrentPv;
    }
    // Move to next item
    m_pCurrentPv = pNextItem;
    pItem = pNextItem;
    return true;
}

void EmPersistentValueIterator::reset() {
    if (m_pCurrentPv != nullptr) {
        delete m_pCurrentPv;
    }
    m_pCurrentPv = nullptr;
}

  //--------------------------------------------------
 // EmPersistentId class implementation   
//--------------------------------------------------
EmPersistentId::EmPersistentId(char a, char b, char c) {
    uint8_t i = 0;
    m_id[i++] = a;
    m_id[i++] = b;
    m_id[i++] = c;
    m_id[i] = 0;
}

EmPersistentId::EmPersistentId(const char* id) {
    if (strlen(id) > c_MaxLen) {
        // ID longer that c_MaxLen chars!
        m_id[c_MaxLen] = 0;
    }
    for(uint8_t i=0; i < c_MaxLen; i++) {
        m_id[i] = strlen(id) > i ? id[i] : 0;
    }
    m_id[c_MaxLen] = 0;
}

EmPersistentId::EmPersistentId(const EmPersistentId& id) {
    memcpy(m_id, id.m_id, sizeof(m_id));
}

char EmPersistentId::operator[](const int index) const
 { 
    if (index >= c_MaxLen) {
        //logError("index out of range!");
        return 0;
    }
    return m_id[index]; 
}

bool EmPersistentId::store_(const EmPersistentState& ps, ps_address_t index) const {
    return ps.updateBytes_(index, (const uint8_t*)m_id, c_MaxLen);
}


bool EmPersistentId::read_(const EmPersistentState& ps, ps_address_t index) {
    return ps.readBytes_(index, (uint8_t*)m_id, c_MaxLen);
}

  //--------------------------------------------------
 // EmPersistentValueBase class implementation   
//--------------------------------------------------
EmPersistentValueBase::EmPersistentValueBase(const EmPersistentState& ps,
                                             const char* id,
                                             ps_address_t address,
                                             ps_size_t bufferSize,
                                             void* pInitValue) 
 : m_ps(ps),
   m_id(EmPersistentId(id)),
   m_address(address),
   m_bufferSize(bufferSize),
   m_pValue(pInitValue) {
    if (nullptr == m_pValue) {
        m_pValue = malloc(m_bufferSize);
        memset(m_pValue, 0, m_bufferSize);
    }
}

bool EmPersistentValueBase::store_() const
{
    // Write the ID
    if (!m_id.store_(m_ps, idAddress_())) {
        return false;
    }
    // Write the size
    if (!m_ps.updateBytes_(sizeAddress_(), (const uint8_t*)&m_bufferSize, sizeof(m_bufferSize))) {
        return false;
    }
    // Write the value itself
    return updateValue_();
}

