#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class VotingEventType {
    VoteRegistered,
    VoteVerificationCompleted
};

struct VotingEvent {
    VotingEventType type;
    int voteId = 0;
    int candidateId = 0;
    std::string details;
};

class IVotingObserver {
public:
    virtual ~IVotingObserver() = default;
    virtual void onNotify(const VotingEvent& event) = 0;
};

class VotingEventPublisher {
private:
    std::vector<std::weak_ptr<IVotingObserver>> observers_;

    void pruneExpired() {
        std::erase_if(observers_, [](const std::weak_ptr<IVotingObserver>& wp) {
            return wp.expired();
            });
    }

public:
    void subscribe(const std::shared_ptr<IVotingObserver>& observer) {
        if (!observer) return;
        pruneExpired();
        bool alreadySubscribed = std::any_of(
            observers_.begin(), observers_.end(),
            [&observer](const std::weak_ptr<IVotingObserver>& wp) {
                auto sp = wp.lock();
                return sp && sp == observer;
            }
        );
        if (!alreadySubscribed) {
            observers_.push_back(observer);
        }
    }

    void unsubscribe(const std::shared_ptr<IVotingObserver>& observer) {
        if (!observer) return;
        std::erase_if(observers_, [&observer](const std::weak_ptr<IVotingObserver>& wp) {
            auto sp = wp.lock();
            return !sp || sp == observer;
            });
    }

    void notify(const VotingEvent& event) {
        pruneExpired();
        std::vector<std::shared_ptr<IVotingObserver>> activeSnapshot;
        activeSnapshot.reserve(observers_.size());
        for (const auto& wp : observers_) {
            if (auto sp = wp.lock()) {
                activeSnapshot.push_back(sp);
            }
        }
        for (const auto& obs : activeSnapshot) {
            obs->onNotify(event);
        }
    }
};

// Підписник 1: Журнал голосування
class VotingLogObserver : public IVotingObserver {
private:
    std::vector<std::string> logs_;

public:
    void onNotify(const VotingEvent& event) override {
        std::string evName = (event.type == VotingEventType::VoteRegistered)
            ? "РЕЄСТРАЦІЯ ГОЛОСУ"
            : "ЗАВЕРШЕННЯ ПЕРЕВІРКИ ГОЛОСУ";
        logs_.push_back("[" + evName + "] VoteID=" + std::to_string(event.voteId) +
            ", CandidateID=" + std::to_string(event.candidateId) +
            " (" + event.details + ")");
    }

    const std::vector<std::string>& getLogs() const { return logs_; }
};

// Підписник 2: Статистика результатів
class ResultsStatsObserver : public IVotingObserver {
private:
    int registeredCount_ = 0;
    int verificationCompletedCount_ = 0;
    std::unordered_map<int, int> candidateEventCounts_;

public:
    void onNotify(const VotingEvent& event) override {
        if (event.type == VotingEventType::VoteRegistered) {
            ++registeredCount_;
        }
        else if (event.type == VotingEventType::VoteVerificationCompleted) {
            ++verificationCompletedCount_;
        }
        ++candidateEventCounts_[event.candidateId];
    }

    int getRegisteredCount() const { return registeredCount_; }
    int getVerificationCompletedCount() const { return verificationCompletedCount_; }
    int getEventsForCandidate(int candidateId) const {
        auto it = candidateEventCounts_.find(candidateId);
        return (it != candidateEventCounts_.end()) ? it->second : 0;
    }
};