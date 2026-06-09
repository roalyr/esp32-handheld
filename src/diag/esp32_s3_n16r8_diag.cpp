//
// PROJECT: ESP32-S2-Mini handheld terminal
// MODULE: src/diag/esp32_s3_n16r8_diag.cpp
// STATUS: [Level 2 - Implementation]
// TRUTH_LINK: TACTICAL_TODO TASK_2
// LOG_REF: 2026-05-05
//

#include <Arduino.h>
#include <driver/gpio.h>

#ifndef ESP32_S3_N16R8_DIAG
#error "This standalone diagnostic source must only be built by the esp32-s3-n16r8_diag environment."
#endif

namespace {

enum class ProbeDisposition : uint8_t {
    ProbeCandidate,
    SkipHistoricalReference,
    SkipReserved,
};

struct PinCandidate {
    uint8_t gpio;
    ProbeDisposition disposition;
    const char* role;
    const char* note;
};

constexpr unsigned long kSerialAttachTimeoutMs = 1500;
constexpr unsigned long kSkipDwellMs = 200;
constexpr unsigned long kBlinkLeadInMs = 150;
constexpr unsigned long kBlinkHighMs = 400;
constexpr unsigned long kBlinkLowMs = 400;
constexpr uint8_t kBlinkCycles = 2;

// Historical ESP32-S3 wiring recovered from the initial project revision.
// These pins stay in the manifest as occupied references instead of being
// treated as implicitly safe blink candidates.
constexpr PinCandidate kPinManifest[] = {
    {0, ProbeDisposition::SkipReserved, "boot strap", "keep out of the first diagnostic sweep"},
    {1, ProbeDisposition::SkipHistoricalReference, "historical keypad column", "initial S3 matrix column pin"},
    {2, ProbeDisposition::SkipHistoricalReference, "historical keypad column", "initial S3 matrix column pin"},
    {3, ProbeDisposition::SkipReserved, "boot strap", "ESP32-S3 strapping pin"},
    {4, ProbeDisposition::SkipHistoricalReference, "historical LCD reset", "initial S3 display reset pin"},
    {5, ProbeDisposition::SkipHistoricalReference, "historical LCD data/command", "initial S3 display DC pin"},
    {6, ProbeDisposition::SkipHistoricalReference, "historical keypad column", "initial S3 matrix column pin"},
    {7, ProbeDisposition::SkipHistoricalReference, "historical keypad column", "initial S3 matrix column pin"},
    {8, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {9, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {10, ProbeDisposition::SkipHistoricalReference, "historical LCD chip select", "initial S3 display CS pin"},
    {11, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {12, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {13, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {14, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {15, ProbeDisposition::SkipHistoricalReference, "historical keypad column", "initial S3 matrix column pin"},
    {16, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {17, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {18, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {19, ProbeDisposition::SkipReserved, "native USB D-", "avoid disturbing the USB serial link"},
    {20, ProbeDisposition::SkipReserved, "native USB D+", "avoid disturbing the USB serial link"},
    {21, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {26, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {27, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {28, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {29, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {30, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {31, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {32, ProbeDisposition::SkipReserved, "SPI0/1", "reserved for flash and PSRAM"},
    {33, ProbeDisposition::SkipReserved, "SPI0/1 OPI", "not probe-safe on S3R8 or OPI memory boards"},
    {34, ProbeDisposition::SkipReserved, "SPI0/1 OPI", "not probe-safe on S3R8 or OPI memory boards"},
    {35, ProbeDisposition::SkipReserved, "SPI0/1 OPI", "not probe-safe on S3R8 or OPI memory boards"},
    {36, ProbeDisposition::SkipReserved, "SPI0/1 OPI", "not probe-safe on S3R8 or OPI memory boards"},
    {37, ProbeDisposition::SkipReserved, "SPI0/1 OPI", "not probe-safe on S3R8 or OPI memory boards"},
    {38, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {39, ProbeDisposition::SkipHistoricalReference, "historical keypad row", "initial S3 matrix row pin"},
    {40, ProbeDisposition::SkipHistoricalReference, "historical keypad row", "initial S3 matrix row pin"},
    {41, ProbeDisposition::SkipHistoricalReference, "historical keypad row", "initial S3 matrix row pin"},
    {42, ProbeDisposition::SkipHistoricalReference, "historical keypad row", "initial S3 matrix row pin"},
    {43, ProbeDisposition::SkipReserved, "UART0 TX", "leave ROM and console serial routing untouched during first sweep"},
    {44, ProbeDisposition::SkipReserved, "UART0 RX", "leave ROM and console serial routing untouched during first sweep"},
    {45, ProbeDisposition::SkipReserved, "boot strap", "ESP32-S3 strapping pin"},
    {46, ProbeDisposition::SkipReserved, "boot strap", "ESP32-S3 strapping pin"},
    {47, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
    {48, ProbeDisposition::ProbeCandidate, "candidate", "general GPIO candidate"},
};

size_t nextManifestIndex = 0;
uint32_t completedSweeps = 0;

const char* dispositionLabel(ProbeDisposition disposition) {
    switch (disposition) {
        case ProbeDisposition::ProbeCandidate:
            return "probe";
        case ProbeDisposition::SkipHistoricalReference:
            return "occupied-ref";
        case ProbeDisposition::SkipReserved:
            return "skip";
        default:
            return "unknown";
    }
}

void printManifestSummary() {
    Serial.println();
    Serial.println("ESP32-S3 N16R8 GPIO blink diagnostic");
    Serial.println("Upload path: normal PlatformIO/esptool serial upload.");
    Serial.println("Historical S3 pins remain occupied references and are not auto-probed.");
    for (const PinCandidate& candidate : kPinManifest) {
        Serial.printf("GPIO %u | %s | %s | %s\n",
                      static_cast<unsigned>(candidate.gpio),
                      dispositionLabel(candidate.disposition),
                      candidate.role,
                      candidate.note);
    }
    Serial.println();
}

void neutralizePin(uint8_t gpio) {
    gpio_reset_pin(static_cast<gpio_num_t>(gpio));
}

void reportManifestEntry(size_t index, const PinCandidate& candidate) {
    Serial.printf("[%u/%u] GPIO %u | %s | %s | %s\n",
                  static_cast<unsigned>(index + 1),
                  static_cast<unsigned>(sizeof(kPinManifest) / sizeof(kPinManifest[0])),
                  static_cast<unsigned>(candidate.gpio),
                  dispositionLabel(candidate.disposition),
                  candidate.role,
                  candidate.note);
}

void blinkCandidatePin(const PinCandidate& candidate) {
    reportManifestEntry(nextManifestIndex, candidate);
    Serial.printf("GPIO %u probe start\n", static_cast<unsigned>(candidate.gpio));

    neutralizePin(candidate.gpio);
    pinMode(candidate.gpio, OUTPUT);
    digitalWrite(candidate.gpio, LOW);
    delay(kBlinkLeadInMs);

    for (uint8_t cycle = 0; cycle < kBlinkCycles; ++cycle) {
        Serial.printf("GPIO %u cycle %u HIGH\n",
                      static_cast<unsigned>(candidate.gpio),
                      static_cast<unsigned>(cycle + 1));
        digitalWrite(candidate.gpio, HIGH);
        delay(kBlinkHighMs);

        Serial.printf("GPIO %u cycle %u LOW\n",
                      static_cast<unsigned>(candidate.gpio),
                      static_cast<unsigned>(cycle + 1));
        digitalWrite(candidate.gpio, LOW);
        delay(kBlinkLowMs);
    }

    neutralizePin(candidate.gpio);
    Serial.printf("GPIO %u returned to INPUT\n", static_cast<unsigned>(candidate.gpio));
    Serial.println();
}

void reportSkippedPin(const PinCandidate& candidate) {
    reportManifestEntry(nextManifestIndex, candidate);
    Serial.printf("GPIO %u skipped: %s\n\n",
                  static_cast<unsigned>(candidate.gpio),
                  candidate.note);
    delay(kSkipDwellMs);
}

void runNextManifestEntry() {
    const PinCandidate& candidate = kPinManifest[nextManifestIndex];
    if (candidate.disposition == ProbeDisposition::ProbeCandidate) {
        blinkCandidatePin(candidate);
    } else {
        reportSkippedPin(candidate);
    }

    nextManifestIndex++;
    if (nextManifestIndex >= (sizeof(kPinManifest) / sizeof(kPinManifest[0]))) {
        nextManifestIndex = 0;
        completedSweeps++;
        Serial.printf("Completed diagnostic sweep %lu\n\n",
                      static_cast<unsigned long>(completedSweeps));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    const unsigned long attachStart = millis();
    while (!Serial && (millis() - attachStart) < kSerialAttachTimeoutMs) {
        delay(10);
    }
    printManifestSummary();
}

void loop() {
    runNextManifestEntry();
}