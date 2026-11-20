#pragma once
#include <chrono>
#include <stdexcept>
#include <string>
#include <sqlite_orm/sqlite_orm.h>
#include "Vote.h"
#include "VoteState.h"

template <typename Storage>
class VoteWorkflowService {
private:
    Storage& storage_;

    static long long currentUnixTime() {
        using namespace std::chrono;
        return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    }

    template <typename TransitionFn>
    Vote applyTransition(int voteId, TransitionFn&& fn, bool simulateHistoryFailure = false) {
        auto voteOpt = storage_.template get_optional<Vote>(voteId);
        if (!voteOpt.has_value()) {
            throw std::runtime_error("Голос з ID " + std::to_string(voteId) + " не знайдено.");
        }

        Vote vote = *voteOpt;
        std::string oldStatus = vote.status;

        VoteContext context(vote);
        fn(context);
        Vote updatedVote = context.getVote();
        std::string newStatus = updatedVote.status;

        long long now = currentUnixTime();

        storage_.transaction([&]() {
            storage_.update(updatedVote);

            if (simulateHistoryFailure) {
                throw std::runtime_error("Симуляція збою при записі історії зміни стану голосу.");
            }

            VoteStatusHistory historyEntry{
                0,
                updatedVote.id,
                oldStatus,
                newStatus,
                now
            };
            storage_.insert(historyEntry);
            return true;
            });

        return updatedVote;
    }

public:
    explicit VoteWorkflowService(Storage& storage) : storage_(storage) {}

    Vote verifyVote(int voteId, bool simulateHistoryFailure = false) {
        return applyTransition(voteId, [](VoteContext& ctx) { ctx.verify(); }, simulateHistoryFailure);
    }

    Vote countVote(int voteId, bool simulateHistoryFailure = false) {
        return applyTransition(voteId, [](VoteContext& ctx) { ctx.markCounted(); }, simulateHistoryFailure);
    }

    Vote rejectVote(int voteId, const std::string& reason, bool simulateHistoryFailure = false) {
        return applyTransition(voteId, [&reason](VoteContext& ctx) { ctx.reject(reason); }, simulateHistoryFailure);
    }
};