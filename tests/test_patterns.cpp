#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sqlite_orm/sqlite_orm.h>
#include "Candidate.h"
#include "Vote.h"
#include "CandidateRepository.h"
#include "VoteState.h"
#include "VoteWorkflowService.h"
#include "VoteShareStrategy.h"
#include "VotingObserver.h"
#include <cstdlib>

namespace {

    inline auto createTestStorage(const std::string& filename) {
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

    bool approxEqual(double a, double b, double eps = 1e-6) {
        return std::fabs(a - b) < eps;
    }

    void testStateAndTransactions() {
        const std::string dbFile = "test_voting_state.sqlite";
        std::remove(dbFile.c_str());

        auto storage = createTestStorage(dbFile);
        storage.sync_schema();

        CandidateRepositoryImpl<decltype(storage)> repo(storage);
        int candId = repo.add(Candidate{ 0, "Тест Кандидат", "Партія Тест", 35, 10.0 });
        int voteId = storage.insert(Vote{ 0, candId, "Дільниця №1", 28, 1731600100, "Зареєстрований" });

        VoteWorkflowService<decltype(storage)> service(storage);

        Vote vVerified = service.verifyVote(voteId);
        assert(vVerified.status == "Перевірений");

        Vote vCounted = service.countVote(voteId);
        assert(vCounted.status == "Врахований");

        // Заборонений перехід з "Врахований" у "Відхилений"
        bool threwForbidden = false;
        try {
            service.rejectVote(voteId, "Пізня спроба");
        }
        catch (const std::logic_error&) {
            threwForbidden = true;
        }
        assert(threwForbidden);

        // Атомарність: при збої запису історії статус голосу має відкотитися
        int vote2Id = storage.insert(Vote{ 0, candId, "Дільниця №2", 40, 1731600200, "Зареєстрований" });
        bool threwRollback = false;
        try {
            service.verifyVote(vote2Id, true);
        }
        catch (const std::runtime_error&) {
            threwRollback = true;
        }
        assert(threwRollback);

        auto vote2After = storage.get_optional<Vote>(vote2Id);
        assert(vote2After.has_value() && vote2After->status == "Зареєстрований");

        auto history = storage.get_all<VoteStatusHistory>();
        assert(history.size() == 2);
    }

    void testStrategyAndFactory() {
        VoteShareCalculator calc(VoteShareStrategyFactory::createStrategy(ShareStrategyType::Direct));
        assert(approxEqual(calc.compute(3, 10), 0.3));

        calc.setStrategy(VoteShareStrategyFactory::createStrategy(ShareStrategyType::Ratio));
        assert(approxEqual(calc.compute(3, 10), 0.3));

        calc.setStrategy(VoteShareStrategyFactory::createStrategy(ShareStrategyType::PercentageConverted));
        assert(approxEqual(calc.compute(3, 10), 0.3));

        assert(approxEqual(calc.compute(5, 0), 0.0));
    }

    void testObserver() {
        VotingEventPublisher publisher;
        auto logger = std::make_shared<VotingLogObserver>();
        auto stats = std::make_shared<ResultsStatsObserver>();

        publisher.subscribe(logger);
        publisher.subscribe(stats);

        publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, 1, 10, "Дільниця №1" });
        publisher.notify(VotingEvent{ VotingEventType::VoteVerificationCompleted, 1, 10, "Врахований" });

        assert(logger->getLogs().size() == 2);
        assert(stats->getRegisteredCount() == 1);
        assert(stats->getVerificationCompletedCount() == 1);
        assert(stats->getEventsForCandidate(10) == 2);

        publisher.unsubscribe(logger);
        publisher.notify(VotingEvent{ VotingEventType::VoteRegistered, 2, 10, "Дільниця №2" });

        assert(logger->getLogs().size() == 2);
        assert(stats->getRegisteredCount() == 2);
    }

} // namespace

int main() {
    std::system("chcp 65001 > nul");
    testStateAndTransactions();
    testStrategyAndFactory();
    testObserver();
    std::cout << "Усі модульні тести патернів (State, Strategy, Observer, Factory Method) пройдено успішно!\n";
    return 0;
}