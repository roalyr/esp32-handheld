## CURRENT GOAL: T9 Editor Coding-Comfort Upgrade v1.3
- TARGET_FILE: src/apps/settings.cpp
- TRUTH_RELIANCE: TRUTH_HARDWARE.md Section 0 (ESP32-S2-Mini platform limits), Section 2 (4x5 key matrix and special keys including ESC/TAB/ENTER/arrows); TRUTH_FLASHING.md Quick Reference + Flashing Procedure (build/flash verification flow)
- TECHNICAL_CONSTRAINTS:
  - TRUTH_PROJECT.md is not present in workspace; no additional constraints may be inferred beyond explicit Truth files.
- ATOMIC_TASKS:
  - [ ] TASK_1: Rework T9 editor input/prediction behavior and coding symbols without scope creep.
    Required signatures:
    - src/apps/settings.h:
      - Add enum T9ViewMode { VIEW_FULL, VIEW_FULL_LINENO, VIEW_MIN_LINENO, VIEW_MIN };
      - Add member: T9ViewMode t9ViewMode;
      - Add member: bool t9SavePromptActive;
      - Add member: int t9SavePromptSelection;  // 0=No, 1=Yes
      - Add methods: void t9HandleSavePromptInput(char key); void renderT9SavePrompt();
    - src/t9_predict.h:
      - Add method: int getPrefixCandidateCountForLength(int targetLen) const;
      - Add method: const char* getPrefixCandidateForLength(int targetLen, int index) const;
      - Add method: void nextPrefixCandidateForLength(int targetLen);
      - Add method: void prevPrefixCandidateForLength(int targetLen);
      - Add method: int getSingleKeyLetterCount(char digit) const;
      - Add method: const char* getSingleKeyLetter(char digit, int index) const;
    Required behavior:
    - TAB must no longer insert newline. TAB cycles editor view modes in this exact order:
      - VIEW_FULL: header + footer
      - VIEW_FULL_LINENO: header + footer + line numbers
      - VIEW_MIN_LINENO: no header/footer + line numbers
      - VIEW_MIN: no header/footer + no line numbers
    - In MODE_T9 when current pending input is exactly one keypress (digit 2-9), UP/DOWN cycling order must be:
      1) individual letters on that key in keypad order (2=abc, 3=def, ..., 9=wxyz),
      2) then dictionary suggestions.
    - Prefix suggestion priority must prefer words whose visible output length equals current digit count; only then allow longer completions.
    - Key 1 in editor must include Lua coding symbols (not only punctuation). Minimum symbol pool:
      - .,?!1()[]{}<>:;"'`=+-*/%#_\\|&$
    - Bracket completion: committing left bracket from any input mode inserts matching pair and keeps cursor between, for (), [], {}, <>.

  - [ ] TASK_2: Add authoring-assist content and safe-exit UX contract for T9 editor.
    Required signatures:
    - src/apps/settings.cpp:
      - ESC while in T9 editor opens modal save prompt stub and blocks editor ESC handling until answered.
      - Prompt choices: No / Yes.
      - On No: close prompt and exit editor.
      - On Yes: close prompt, run save stub (no SD write yet), then exit editor.
    - data_backup/t9_lua_typing_sample.lua:
      - Create Lua typing reference sample containing:
        - table literals
        - local functions
        - if/elseif/else
        - for and while loops
        - brackets/parens/braces
        - string quoting and escapes
        - comments and common operators
    Required behavior:
    - While save prompt is active, only LEFT/RIGHT/UP/DOWN/TAB/ENTER/ESC for prompt navigation are accepted; regular text entry and normal editor ESC path are blocked.
    - Save path is explicit stub only (for now): log intent and return success state without filesystem writes.

  - [ ] VERIFICATION: Build + interaction checklist
    Success criteria/tests to run:
    - pio run -e lolin_s2_mini completes with zero errors.
    - T9 editor TAB cycles exactly 4 visual modes and never inserts newline.
    - In T9 mode, single digit 6 cycles m -> n -> o before any dictionary candidate.
    - Prefix candidate ordering for 2-digit entry favors 2-char outputs before longer words.
    - Key 1 cycles through Lua coding symbol pool and renders correctly.
    - Bracket completion inserts pairs and leaves cursor centered between brackets.
    - ESC opens blocking save prompt; editor cannot be exited until No/Yes chosen.
    - Choosing Yes executes save stub path (no SD I/O) and exits cleanly.
