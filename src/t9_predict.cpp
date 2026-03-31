//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/t9_predict.cpp
// Description: T9 predictive text lookup — binary search on PROGMEM dictionary.
//

#include "t9_predict.h"
#include "t9_dict.h"
#include <pgmspace.h>

T9Predict::T9Predict() {
    reset();
}

void T9Predict::reset() {
    digits[0] = '\0';
    digitCount = 0;
    selectedIdx = 0;
    candidateCount = 0;
    indexPos = -1;
}

void T9Predict::pushDigit(char digit) {
    if (digit < '2' || digit > '9') return;
    if (digitCount >= 15) return;
    digits[digitCount++] = digit;
    digits[digitCount] = '\0';
    selectedIdx = 0;
    updateCandidates();
}

void T9Predict::popDigit() {
    if (digitCount <= 0) return;
    digitCount--;
    digits[digitCount] = '\0';
    selectedIdx = 0;
    updateCandidates();
}

const char* T9Predict::getDigits() const {
    return digits;
}

int T9Predict::getDigitCount() const {
    return digitCount;
}

bool T9Predict::hasInput() const {
    return digitCount > 0;
}

int T9Predict::getCandidateCount() const {
    return candidateCount;
}

int T9Predict::getSelectedIndex() const {
    return selectedIdx;
}

void T9Predict::nextCandidate() {
    if (candidateCount > 0) {
        selectedIdx = (selectedIdx + 1) % candidateCount;
    }
}

void T9Predict::prevCandidate() {
    if (candidateCount > 0) {
        selectedIdx = (selectedIdx - 1 + candidateCount) % candidateCount;
    }
}

const char* T9Predict::getCandidate(int index) const {
    if (indexPos < 0 || index < 0 || index >= candidateCount) return nullptr;

    // Read entry from PROGMEM
    T9IndexEntry entry;
    memcpy_P(&entry, &t9_index[indexPos], sizeof(T9IndexEntry));

    // Walk through null-terminated words in the pool
    uint16_t pos = entry.offset;
    for (int i = 0; i < index; i++) {
        // Skip to next word
        while (pos < T9_WORD_POOL_SIZE && pgm_read_byte(&t9_word_pool[pos]) != 0) {
            pos++;
        }
        pos++; // skip null terminator
    }

    if (pos >= T9_WORD_POOL_SIZE) return nullptr;

    // Copy word from PROGMEM to static buffer
    static char buf[16];
    int len = 0;
    while (len < 15 && pos + len < T9_WORD_POOL_SIZE) {
        char c = pgm_read_byte(&t9_word_pool[pos + len]);
        if (c == 0) break;
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

const char* T9Predict::getSelectedWord() const {
    if (!hasInput() || candidateCount == 0) return nullptr;
    return getCandidate(selectedIdx);
}

// --- Private helpers ---

uint32_t T9Predict::digitsToKey() const {
    uint32_t key = 0;
    for (int i = 0; i < digitCount; i++) {
        key = key * 10 + (digits[i] - '0');
    }
    return key;
}

int T9Predict::binarySearch(uint32_t key) const {
    int lo = 0;
    int hi = T9_INDEX_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[mid], sizeof(T9IndexEntry));
        if (entry.key == key) return mid;
        if (entry.key < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

void T9Predict::updateCandidates() {
    if (digitCount == 0) {
        candidateCount = 0;
        indexPos = -1;
        return;
    }

    uint32_t key = digitsToKey();
    indexPos = binarySearch(key);

    if (indexPos >= 0) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[indexPos], sizeof(T9IndexEntry));
        candidateCount = entry.count;
    } else {
        candidateCount = 0;
    }

    if (selectedIdx >= candidateCount) {
        selectedIdx = 0;
    }
}
