//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/t9_predict.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_1
// LOG_REF: 2026-04-22
// Description: T9 predictive text lookup — binary search on PROGMEM dictionary.
//              Prefix matching scans sorted index for incremental suggestions.
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
    prefixCandidateCount = 0;
    prefixSelectedIdx = 0;
}

void T9Predict::pushDigit(char digit) {
    if (digit < '2' || digit > '9') return;
    if (digitCount >= 15) return;
    digits[digitCount++] = digit;
    digits[digitCount] = '\0';
    selectedIdx = 0;
    prefixSelectedIdx = 0;
    updateCandidates();
}

void T9Predict::popDigit() {
    if (digitCount <= 0) return;
    digitCount--;
    digits[digitCount] = '\0';
    selectedIdx = 0;
    prefixSelectedIdx = 0;
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
    uint32_t pos = entry.offset;
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

// --- Prefix candidate API ---

int T9Predict::getPrefixCandidateCount() const {
    return prefixCandidateCount;
}

const char* T9Predict::getPrefixCandidate(int index) const {
    if (index < 0 || index >= prefixCandidateCount) return nullptr;

    int iPos = prefixMatches[index].indexPos;
    int wIdx = prefixMatches[index].wordIdx;

    T9IndexEntry entry;
    memcpy_P(&entry, &t9_index[iPos], sizeof(T9IndexEntry));

    // Walk to the wIdx-th word in the pool for this entry
    uint32_t pos = entry.offset;
    for (int i = 0; i < wIdx; i++) {
        while (pos < T9_WORD_POOL_SIZE && pgm_read_byte(&t9_word_pool[pos]) != 0) {
            pos++;
        }
        pos++;
    }

    if (pos >= T9_WORD_POOL_SIZE) return nullptr;

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

const char* T9Predict::getSelectedPrefixWord() const {
    if (!hasInput() || prefixCandidateCount == 0) return nullptr;
    return getPrefixCandidate(prefixSelectedIdx);
}

int T9Predict::getSelectedPrefixIndex() const {
    return prefixSelectedIdx;
}

void T9Predict::nextPrefixCandidate() {
    if (prefixCandidateCount > 0) {
        prefixSelectedIdx = (prefixSelectedIdx + 1) % prefixCandidateCount;
    }
}

void T9Predict::prevPrefixCandidate() {
    if (prefixCandidateCount > 0) {
        prefixSelectedIdx = (prefixSelectedIdx - 1 + prefixCandidateCount) % prefixCandidateCount;
    }
}

int T9Predict::getPrefixCandidateCountForLength(int targetLen) const {
    if (targetLen <= 0) return 0;
    int count = 0;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) == targetLen) count++;
    }
    return count;
}

const char* T9Predict::getPrefixCandidateForLength(int targetLen, int index) const {
    if (targetLen <= 0 || index < 0) return nullptr;
    int seen = 0;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) != targetLen) continue;
        if (seen == index) return getPrefixCandidate(i);
        seen++;
    }
    return nullptr;
}

void T9Predict::nextPrefixCandidateForLength(int targetLen) {
    if (targetLen <= 0 || prefixCandidateCount == 0) return;
    int count = getPrefixCandidateCountForLength(targetLen);
    if (count <= 0) return;

    int filteredIdx = 0;
    bool foundCurrent = false;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) != targetLen) continue;
        if (i == prefixSelectedIdx) {
            foundCurrent = true;
            break;
        }
        filteredIdx++;
    }
    if (!foundCurrent) filteredIdx = 0;
    int nextFiltered = (filteredIdx + 1) % count;

    int seen = 0;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) != targetLen) continue;
        if (seen == nextFiltered) {
            prefixSelectedIdx = i;
            return;
        }
        seen++;
    }
}

void T9Predict::prevPrefixCandidateForLength(int targetLen) {
    if (targetLen <= 0 || prefixCandidateCount == 0) return;
    int count = getPrefixCandidateCountForLength(targetLen);
    if (count <= 0) return;

    int filteredIdx = 0;
    bool foundCurrent = false;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) != targetLen) continue;
        if (i == prefixSelectedIdx) {
            foundCurrent = true;
            break;
        }
        filteredIdx++;
    }
    if (!foundCurrent) filteredIdx = 0;
    int prevFiltered = (filteredIdx - 1 + count) % count;

    int seen = 0;
    for (int i = 0; i < prefixCandidateCount; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[prefixMatches[i].indexPos], sizeof(T9IndexEntry));
        if (keyDigitCount(entry.key) != targetLen) continue;
        if (seen == prevFiltered) {
            prefixSelectedIdx = i;
            return;
        }
        seen++;
    }
}

int T9Predict::getSingleKeyLetterCount(char digit) const {
    if (digit < '2' || digit > '9') return 0;
    switch (digit) {
        case '7':
        case '9':
            return 4;
        default:
            return 3;
    }
}

const char* T9Predict::getSingleKeyLetter(char digit, int index) const {
    static const char* letters[] = {
        "", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"
    };
    static char out[2];
    if (digit < '2' || digit > '9') return nullptr;
    const char* set = letters[digit - '0'];
    int count = getSingleKeyLetterCount(digit);
    if (index < 0 || index >= count) return nullptr;
    out[0] = set[index];
    out[1] = '\0';
    return out;
}

// --- Private helpers ---

// All keys are left-aligned to 15 decimal digits.
// e.g. digits "7663" -> 766300000000000ULL.
// This guarantees that prefix ordering is preserved in integer sort:
// all entries whose digit string starts with "7663" form a
// contiguous range [766300000000000, 766400000000000).

static const int KEY_WIDTH = 15;

// Pre-computed powers of 10
static uint64_t pow10(int n) {
    uint64_t r = 1;
    for (int i = 0; i < n; i++) r *= 10;
    return r;
}

uint64_t T9Predict::digitsToKey() const {
    uint64_t key = 0;
    for (int i = 0; i < digitCount; i++) {
        key = key * 10 + (digits[i] - '0');
    }
    // Left-align to KEY_WIDTH digits
    key *= pow10(KEY_WIDTH - digitCount);
    return key;
}

int T9Predict::binarySearch(uint64_t key) const {
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
        prefixCandidateCount = 0;
        prefixSelectedIdx = 0;
        return;
    }

    uint64_t key = digitsToKey();
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

    updatePrefixCandidates();
}

// With left-aligned keys, prefix matching is a simple range check.
// All keys sharing the same N-digit prefix fall in a contiguous range.
// Not used with left-aligned keys, but kept for API compatibility.
int T9Predict::keyDigitCount(uint64_t key) {
    if (key == 0) return 0;
    int trailingZeros = 0;
    while ((key % 10ULL) == 0ULL && trailingZeros < KEY_WIDTH) {
        trailingZeros++;
        key /= 10ULL;
    }
    return KEY_WIDTH - trailingZeros;
}

bool T9Predict::keyStartsWith(uint64_t key, uint64_t prefix, int prefixLen) {
    (void)prefixLen; // unused with left-aligned keys
    // Not used in the new prefix scan; kept for compatibility
    return key >= prefix && key < prefix + pow10(KEY_WIDTH - prefixLen);
}

void T9Predict::updatePrefixCandidates() {
    prefixCandidateCount = 0;
    prefixSelectedIdx = 0;

    if (digitCount == 0) return;

    uint64_t prefixKey = digitsToKey();  // already left-aligned
    // All matching keys are in [prefixKey, prefixKey + span)
    uint64_t span = pow10(KEY_WIDTH - digitCount);
    uint64_t rangeEnd = prefixKey + span;

    // Binary search for the first index entry >= prefixKey
    int lo = 0, hi = (int)T9_INDEX_COUNT - 1, firstIdx = (int)T9_INDEX_COUNT;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[mid], sizeof(T9IndexEntry));
        if (entry.key >= prefixKey) { firstIdx = mid; hi = mid - 1; }
        else lo = mid + 1;
    }

    // Pass 1: exact-length matches first.
    for (int i = firstIdx; i < (int)T9_INDEX_COUNT && prefixCandidateCount < MAX_PREFIX_MATCHES; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[i], sizeof(T9IndexEntry));
        if (entry.key >= rangeEnd) break;
        if (keyDigitCount(entry.key) != digitCount) continue;
        prefixMatches[prefixCandidateCount].indexPos = i;
        prefixMatches[prefixCandidateCount].wordIdx = 0;
        prefixCandidateCount++;
    }

    // Pass 2: longer keys that share the same prefix.
    for (int i = firstIdx; i < (int)T9_INDEX_COUNT && prefixCandidateCount < MAX_PREFIX_MATCHES; i++) {
        T9IndexEntry entry;
        memcpy_P(&entry, &t9_index[i], sizeof(T9IndexEntry));
        if (entry.key >= rangeEnd) break;
        if (keyDigitCount(entry.key) <= digitCount) continue;
        prefixMatches[prefixCandidateCount].indexPos = i;
        prefixMatches[prefixCandidateCount].wordIdx = 0;
        prefixCandidateCount++;
    }
}
