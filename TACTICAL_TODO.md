## CURRENT GOAL: Add ESP32-S2 & ESP32-S3 Build & Flash Scripts (Completed)
- TARGET_SCOPE: Create scripts/build_s2.sh, scripts/build_s3.sh, and scripts/flash_s3.sh.
- TARGET_FILES: scripts/build_s2.sh, scripts/build_s3.sh, scripts/flash_s3.sh, TRUTH_FLASHING.md
- TRUTH_RELIANCE: TRUTH_FLASHING.md
- TECHNICAL_CONSTRAINTS: N/A
- ATOMIC_TASKS:
  - [x] TASK_1: Create build_s2.sh script to compile lolin_s2_mini target.
  - [x] TASK_2: Create build_s3.sh script to compile esp32-s3-n16r8_diag target.
  - [x] TASK_3: Create flash_s3.sh script using PlatformIO upload target (no-DFU serial upload).
  - [x] VERIFICATION: Test compiled environment builds successfully via scripts.

