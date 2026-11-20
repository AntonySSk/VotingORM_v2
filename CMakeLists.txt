cmake_minimum_required(VERSION 3.21)
project(VotingORM LANGUAGES CXX)

add_compile_options(/utf-8)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Пошук підключених через vcpkg бібліотек
find_package(SqliteOrm CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Основний виконуваний файл застосунку
add_executable(VotingORM
    src/main.cpp
    include/Candidate.h
    include/Vote.h
    include/ICandidateRepository.h
    include/CandidateRepository.h
    include/VoteState.h
    include/VoteWorkflowService.h
    include/VoteShareStrategy.h
    include/VotingObserver.h
)

target_include_directories(VotingORM
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(VotingORM
    PRIVATE
    sqlite_orm::sqlite_orm
    unofficial::sqlite3::sqlite3
)

# Ціль модульних тестів
enable_testing()

add_executable(VotingTests
    tests/test_patterns.cpp
    include/Candidate.h
    include/Vote.h
    include/ICandidateRepository.h
    include/CandidateRepository.h
    include/VoteState.h
    include/VoteWorkflowService.h
    include/VoteShareStrategy.h
    include/VotingObserver.h
)

target_include_directories(VotingTests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(VotingTests
    PRIVATE
    sqlite_orm::sqlite_orm
    unofficial::sqlite3::sqlite3
)

add_test(NAME VotingPatternsTests COMMAND VotingTests)