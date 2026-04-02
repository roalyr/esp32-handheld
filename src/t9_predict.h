//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/t9_predict.h
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-04-01
// Description: T9 predictive text lookup engine.
//              Maps digit sequences to dictionary words via binary search.
//              Prefix matching for incremental word suggestion.
//

#ifndef T9_PREDICT_H
#define T9_PREDICT_H

#include <Arduino.h>

class T9Predict {
public:
    T9Predict();

    // Reset all state (digit sequence + selection)
    void reset();

    // Add digit (2-9) to current sequence
    void pushDigit(char digit);

    // Remove last digit from sequence
    void popDigit();

    // Current digit sequence
    const char* getDigits() const;
    int getDigitCount() const;
    bool hasInput() const;

    // Exact-match candidates for current digit sequence
    int getCandidateCount() const;
    const char* getCandidate(int index) const;
    int getSelectedIndex() const;
    void nextCandidate();
    void prevCandidate();
    const char* getSelectedWord() const;

    // Prefix-match candidates (words whose digit sequence starts with current digits)
    int getPrefixCandidateCount() const;
    const char* getPrefixCandidate(int index) const;
    const char* getSelectedPrefixWord() const;
    int getSelectedPrefixIndex() const;
    void nextPrefixCandidate();
    void prevPrefixCandidate();

private:
    char digits[16];        // Current digit sequence (null-terminated)
    int digitCount;         // Number of digits entered
    int selectedIdx;        // Currently selected exact candidate
    int candidateCount;     // Exact-match candidates found
    int indexPos;           // Position in t9_index for exact match (-1 = none)

    // Prefix matching state
    struct PrefixMatch { int indexPos; int wordIdx; };
    static const int MAX_PREFIX_MATCHES = 64;
    PrefixMatch prefixMatches[MAX_PREFIX_MATCHES];
    int prefixCandidateCount;
    int prefixSelectedIdx;

    void updateCandidates();
    void updatePrefixCandidates();
    int binarySearch(uint32_t key) const;
    uint32_t digitsToKey() const;
    static int keyDigitCount(uint32_t key);
    static bool keyStartsWith(uint32_t key, uint32_t prefix, int prefixLen);
};

#endif
