#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include "Vote.h"

class VoteContext;

class IVoteState {
public:
    virtual ~IVoteState() = default;

    virtual std::string name() const = 0;
    virtual void verify(VoteContext& ctx) = 0;
    virtual void markCounted(VoteContext& ctx) = 0;
    virtual void reject(VoteContext& ctx, const std::string& reason) = 0;
};

class VoteContext {
private:
    Vote vote_;
    std::unique_ptr<IVoteState> state_;

    static std::unique_ptr<IVoteState> makeStateByName(const std::string& statusName);

public:
    explicit VoteContext(Vote vote);

    const Vote& getVote() const { return vote_; }
    Vote& getVoteMutable() { return vote_; }

    std::string currentStateName() const {
        return state_ ? state_->name() : vote_.status;
    }

    void setState(std::unique_ptr<IVoteState> newState) {
        state_ = std::move(newState);
        if (state_) {
            vote_.status = state_->name();
        }
    }

    void verify() { state_->verify(*this); }
    void markCounted() { state_->markCounted(*this); }
    void reject(const std::string& reason) { state_->reject(*this, reason); }
};

class RegisteredState : public IVoteState {
public:
    std::string name() const override { return "Зареєстрований"; }
    void verify(VoteContext& ctx) override;
    void markCounted(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: голос має бути спочатку перевірений.");
    }
    void reject(VoteContext& ctx, const std::string& reason) override;
};

class VerifiedState : public IVoteState {
public:
    std::string name() const override { return "Перевірений"; }
    void verify(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: голос уже перевірений.");
    }
    void markCounted(VoteContext& ctx) override;
    void reject(VoteContext& ctx, const std::string& reason) override;
};

class CountedState : public IVoteState {
public:
    std::string name() const override { return "Врахований"; }
    void verify(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: врахований голос не підлягає повторній перевірці.");
    }
    void markCounted(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: голос уже врахований.");
    }
    void reject(VoteContext&, const std::string&) override {
        throw std::logic_error("Недопустимий перехід: врахований голос не можна відхилити.");
    }
};

class RejectedState : public IVoteState {
public:
    std::string name() const override { return "Відхилений"; }
    void verify(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: відхилений голос не можна перевірити.");
    }
    void markCounted(VoteContext&) override {
        throw std::logic_error("Недопустимий перехід: відхилений голос не можна врахувати.");
    }
    void reject(VoteContext&, const std::string&) override {
        throw std::logic_error("Недопустимий перехід: голос уже відхилений.");
    }
};

inline std::unique_ptr<IVoteState> VoteContext::makeStateByName(const std::string& statusName) {
    if (statusName == "Зареєстрований") return std::make_unique<RegisteredState>();
    if (statusName == "Перевірений") return std::make_unique<VerifiedState>();
    if (statusName == "Врахований") return std::make_unique<CountedState>();
    if (statusName == "Відхилений") return std::make_unique<RejectedState>();
    throw std::invalid_argument("Невідомий стан голосу: " + statusName);
}

inline VoteContext::VoteContext(Vote vote)
    : vote_(std::move(vote)), state_(makeStateByName(vote_.status)) {
}

inline void RegisteredState::verify(VoteContext& ctx) {
    ctx.setState(std::make_unique<VerifiedState>());
}

inline void RegisteredState::reject(VoteContext& ctx, const std::string& reason) {
    if (reason.empty()) {
        throw std::invalid_argument("Причина відхилення голосу не може бути порожньою.");
    }
    ctx.setState(std::make_unique<RejectedState>());
}

inline void VerifiedState::markCounted(VoteContext& ctx) {
    ctx.setState(std::make_unique<CountedState>());
}

inline void VerifiedState::reject(VoteContext& ctx, const std::string& reason) {
    if (reason.empty()) {
        throw std::invalid_argument("Причина відхилення голосу не може бути порожньою.");
    }
    ctx.setState(std::make_unique<RejectedState>());
}