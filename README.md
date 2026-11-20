# VotingORM — Система обліку результатів голосування

Консольний застосунок на мові **C++20**, що реалізує роботу з базою даних SQLite через бібліотеку **`sqlite_orm`**, паттерн **Repository** та механізм **транзакцій** з журналюванням історії змін.

## Реалізовані патерни проєктування
- **State** (`include/VoteState.h`, `include/VoteWorkflowService.h`) - керування станами голосу (`Зареєстрований`, `Перевірений`, `Врахований`, `Відхилений`), контроль допустимості переходів та збереження історії змін у таблиці `vote_status_history` у межах транзакції БД.
- **Strategy** (`include/VoteShareStrategy.h`) - три взаємозамінні алгоритми обчислення частки голосів кандидата з можливістю зміни під час виконання програми.
- **Observer** (`include/VotingObserver.h`) - видавець подій та незалежні підписники (`VotingLogObserver`, `ResultsStatsObserver`) із підтримкою підписки й відписки.
- **Factory Method** (`include/VoteShareStrategy.h`) - створення стратегій обчислення частки голосів через `VoteShareStrategyFactory`.

## Вимоги для збирання
- Компілятор C++ із підтримкою **C++20** (наприклад, MSVC у Visual Studio 2026).
- Система збирання **CMake** (версія 3.21 або новіша).
- Пакетний менеджер **vcpkg** у режимі маніфесту (`vcpkg.json`).

## Структура проєкту
- `include/` - моделі даних, репозиторій, сервіс переходів стану та реалізації патернів (`Candidate.h`, `Vote.h`, `ICandidateRepository.h`, `CandidateRepository.h`, `VoteState.h`, `VoteWorkflowService.h`, `VoteShareStrategy.h`, `VotingObserver.h`).
- `src/` - точка входу та демонстрація роботи системи (`main.cpp`)[cite: 1].
- `tests/` - модульні тести патернів і транзакцій (`test_patterns.cpp`).
- `vcpkg.json` - маніфест залежностей (`sqlite3`, `sqlite-orm`).
- `CMakeLists.txt` - конфігурація збирання основного застосунку та цілі тестів `VotingTests`.

## Інструкція зі збирання та запуску (у Visual Studio / PowerShell)

1. Відкрийте папку проєкту в Visual Studio або перейдіть до неї у терміналі/PowerShell[cite: 1].
2. Згенеруйте / оновіть конфігурацію CMake з підключенням `vcpkg` (у Visual Studio це виконується автоматично при відкритті проєкту або після зміни `CMakeLists.txt`).
3. Зберіть цілі `VotingORM` та `VotingTests`.
4. Запустіть `VotingORM.exe` для перевірки основного сценарію роботи та `VotingTests.exe` для запуску модульних тестів[cite: 4, 5].
