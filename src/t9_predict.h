//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/t9_predict.h
// Description: T9 predictive text lookup engine.
//              Maps digit sequences to dictionary words via binary search.
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

    // Candidates for current digit sequence
    int getCandidateCount() const;

    // Get candidate word by index (0 = most likely). Returns nullptr if invalid.
    // Returned pointer is to a static buffer (valid until next call).
    const char* getCandidate(int index) const;

    // Selected candidate cycling
    int getSelectedIndex() const;
    void nextCandidate();
    void prevCandidate();

    // Convenience: get currently selected word (nullptr if none)
    const char* getSelectedWord() const;

private:
    char digits[16];        // Current digit sequence (null-terminated)
    int digitCount;         // Number of digits entered
    int selectedIdx;        // Currently selected candidate
    int candidateCount;     // Candidates found for current sequence
    int indexPos;           // Position in t9_index for current match (-1 = none)

    void updateCandidates();
    int binarySearch(uint32_t key) const;
    uint32_t digitsToKey() const;
};

#endif
