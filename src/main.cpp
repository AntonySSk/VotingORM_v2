#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <cstdio>
#include <sqlite_orm/sqlite_orm.h>
#include "Candidate.h"
#include "Vote.h"
#include "ICandidateRepository.h"
#include "CandidateRepository.h"
#include "VoteState.h"
#include "VoteWorkflowService.h"
#include "VoteShareStrategy.h"
#include "VotingObserver.h"

inline auto createStorage(const std::string& filename) {
    return sqlite_orm::make_storage(filename,
        sqlite_orm::make_table("candidates",
            sqlite_orm::make_column("id", &Candidate::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("full_name", &Candidate::fullName),
            sqlite_orm::make_column("party_name", &Candidate::partyName),
            sqlite_orm::make_column("age", &Candidate::age),
            sqlite_orm::make_column("rating_score", &Candidate::ratingScore)
        ),
        sqlite_orm::make_table("votes",
            sqlite_orm::make_column("id", &Vote::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("candidate_id", &Vote::candidateId),
            sqlite_orm::make_column("voting_station", &Vote::votingStation),
            sqlite_orm::make_column("voter_age", &Vote::voterAge),
            sqlite_orm::make_column("timestamp", &Vote::timestamp),
            sqlite_orm::make_column("status", &Vote::status),
            sqlite_orm::foreign_key(&Vote::candidateId).references(&Candidate::id)
        ),
        sqlite_orm::make_table("vote_status_history",
            sqlite_orm::make_column("id", &VoteStatusHistory::id, sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("vote_id", &VoteStatusHistory::voteId),
            sqlite_orm::make_column("old_status", &VoteStatusHistory::oldStatus),
            sqlite_orm::make_column("new_status", &VoteStatusHistory::newStatus),
            sqlite_orm::make_column("timestamp", &VoteStatusHistory::timestamp),
            sqlite_orm::foreign_key(&VoteStatusHistory::voteId).references(&Vote::id)
        )
    );
}

int main() {
    system("chcp 65001 > nul");

    std::string dbFile = "voting_system.sqlite";
    std::remove(dbFile.c_str());

    auto storage = createStorage(dbFile);
    storage.sync_schema();

    CandidateRepositoryImpl<decltype(storage)> candidateRepo(storage);
    VoteWorkflowService<decltype(storage)> workflow(storage);

    // Observer: реєстрація подій та підписників
    VotingEventPublisher publisher;
    auto logObserver = std::make_shared<VotingLogObserver>();
    auto statsObserver = std::make_shared<ResultsStatsObserver>();
    publisher.subscribe(logObserver);
    publisher.subscribe(statsObserver);

    std::cout << "=== 1. СТВОРЕННЯ КАНДИДАТІВ ТА РЕЄСТРАЦІЯ ГОЛОСІВ (REPOSITORY + OBSERVER) ===\n";
    int c1Id = candidateRepo.add(Candidate{ 0, "Іван Петренко", "Партія Майбутнього", 42, 15.5 });
    int c2Id = candidateRepo.add(Candidate{ 0, "Олена Коваленко", "Свобода Слова", 38, 24.8 });

    int v1Id = storage.insert(Vote{ 0, c1Id, "Дільниця №101", 25, 1731600001, "Зареєстрований" });
    publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, v1Id, c1Id, "Дільниця №101" });

    int v2Id = storage.insert(Vote{ 0, c1Id, "Дільниця №102", 45, 1731600002, "Зареєстрований" });
    publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, v2Id, c1Id, "Дільниця №102" });

    int v3Id = storage.insert(Vote{ 0, c2Id, "Дільниця №101", 30, 1731600003, "Зареєстрований" });
    publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, v3Id, c2Id, "Дільниця №101" });

    int v4Id = storage.insert(Vote{ 0, c2Id, "Дільниця №103", 60, 1731600004, "Зареєстрований" });
    publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, v4Id, c2Id, "Дільниця №103" });

    std::cout << "\n=== 2. КЕРУВАННЯ СТАНАМИ ГОЛОСІВ (STATE + ТРАНЗАКЦІЇ + ІСТОРІЯ) ===\n";
    workflow.verifyVote(v1Id);
    workflow.countVote(v1Id);
    publisher.notify(VotingEvent{ VotingEventType::VoteVerificationCompleted, v1Id, c1Id, "Врахований" });

    workflow.verifyVote(v2Id);
    workflow.countVote(v2Id);
    publisher.notify(VotingEvent{ VotingEventType::VoteVerificationCompleted, v2Id, c1Id, "Врахований" });

    workflow.verifyVote(v3Id);
    workflow.countVote(v3Id);
    publisher.notify(VotingEvent{ VotingEventType::VoteVerificationCompleted, v3Id, c2Id, "Врахований" });

    workflow.rejectVote(v4Id, "Некоректний бюлетень");
    publisher.notify(VotingEvent{ VotingEventType::VoteVerificationCompleted, v4Id, c2Id, "Відхилений" });

    // Перевірка заборони некоректного переходу
    try {
        workflow.countVote(v4Id);
    }
    catch (const std::exception& ex) {
        std::cout << "  [КОНТРОЛЬ СТАНУ] " << ex.what() << "\n";
    }

    // Перевірка атомарності відкату при збої запису історії
    int v5Id = storage.insert(Vote{ 0, c1Id, "Дільниця №104", 29, 1731600005, "Зареєстрований" });
    try {
        workflow.verifyVote(v5Id, true);
    }
    catch (const std::exception& ex) {
        auto v5AfterRollback = storage.get_optional<Vote>(v5Id);
        std::cout << "  [ROLLBACK] " << ex.what()
            << " | Статус голосу після відкату: "
            << (v5AfterRollback ? v5AfterRollback->status : "невідомо") << "\n";
    }

    auto historyRows = storage.get_all<VoteStatusHistory>();
    std::cout << "  Записів у vote_status_history: " << historyRows.size() << "\n";
    for (const auto& h : historyRows) {
        std::cout << "   - VoteID=" << h.voteId << ": " << h.oldStatus << " -> " << h.newStatus << "\n";
    }

    std::cout << "\n=== 3. РОЗРАХУНОК ЧАСТКИ ГОЛОСІВ (STRATEGY + FACTORY METHOD) ===\n";
    int c1CountedVotes = storage.get_all<Vote>(
        sqlite_orm::where(sqlite_orm::c(&Vote::candidateId) == c1Id && sqlite_orm::c(&Vote::status) == "Врахований")
    ).size();
    int totalCountedVotes = storage.get_all<Vote>(
        sqlite_orm::where(sqlite_orm::c(&Vote::status) == "Врахований")
    ).size();

    VoteShareCalculator calculator(VoteShareStrategyFactory::createStrategy(ShareStrategyType::Direct));
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  [" << calculator.currentStrategyName() << "] частка кандидата " << c1Id
        << ": " << calculator.compute(c1CountedVotes, totalCountedVotes) << "\n";

    calculator.setStrategy(VoteShareStrategyFactory::createStrategy(ShareStrategyType::Ratio));
    std::cout << "  [" << calculator.currentStrategyName() << "] частка кандидата " << c1Id
        << ": " << calculator.compute(c1CountedVotes, totalCountedVotes) << "\n";

    calculator.setStrategy(VoteShareStrategyFactory::createStrategy(ShareStrategyType::PercentageConverted));
    std::cout << "  [" << calculator.currentStrategyName() << "] частка кандидата " << c1Id
        << ": " << calculator.compute(c1CountedVotes, totalCountedVotes) << "\n";

    std::cout << "\n=== 4. ПЕРЕВІРКА ПІДПИСНИКІВ (OBSERVER: SUBSCRIBE / UNSUBSCRIBE) ===\n";
    std::cout << "  Зареєстровано подій: " << statsObserver->getRegisteredCount()
        << ", завершено перевірок: " << statsObserver->getVerificationCompletedCount() << "\n";
    publisher.unsubscribe(statsObserver);
    int v6Id = storage.insert(Vote{ 0, c2Id, "Дільниця №105", 35, 1731600006, "Зареєстрований" });
    publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, v6Id, c2Id, "Дільниця №105" });
    std::cout << "  Після відписки statsObserver кількість зареєстрованих подій залишилась: "
        << statsObserver->getRegisteredCount() << "\n";
    std::cout << "  Журнал голосування містить записів: " << logObserver->getLogs().size() << "\n";

    return 0;
}