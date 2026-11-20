#pragma once
#include <memory>
#include <stdexcept>
#include <string>

enum class ShareStrategyType {
    Direct,
    Ratio,
    PercentageConverted
};

class IVoteShareStrategy {
public:
    virtual ~IVoteShareStrategy() = default;
    virtual std::string name() const = 0;
    virtual double calculateShare(int candidateVotes, int totalCountedVotes) const = 0;
};

class DirectShareStrategy : public IVoteShareStrategy {
public:
    std::string name() const override { return "Прямий розрахунок частки"; }

    double calculateShare(int candidateVotes, int totalCountedVotes) const override {
        if (totalCountedVotes <= 0) return 0.0;
        if (candidateVotes < 0) throw std::invalid_argument("Кількість голосів не може бути від'ємною.");
        return static_cast<double>(candidateVotes) / static_cast<double>(totalCountedVotes);
    }
};

class RatioShareStrategy : public IVoteShareStrategy {
public:
    std::string name() const override { return "Розрахунок через відношення до загальної кількості"; }

    double calculateShare(int candidateVotes, int totalCountedVotes) const override {
        if (totalCountedVotes <= 0) return 0.0;
        if (candidateVotes < 0) throw std::invalid_argument("Кількість голосів не може бути від'ємною.");
        const double total = static_cast<double>(totalCountedVotes);
        const double cand = static_cast<double>(candidateVotes);
        return (cand / 1.0) / total;
    }
};

class PercentageToShareStrategy : public IVoteShareStrategy {
public:
    std::string name() const override { return "Розрахунок у відсотках із перетворенням у частку"; }

    double calculateShare(int candidateVotes, int totalCountedVotes) const override {
        if (totalCountedVotes <= 0) return 0.0;
        if (candidateVotes < 0) throw std::invalid_argument("Кількість голосів не може бути від'ємною.");
        const double percent = (static_cast<double>(candidateVotes) * 100.0) / static_cast<double>(totalCountedVotes);
        return percent / 100.0;
    }
};

// Додатковий патерн GoF (Creational): Factory Method для створення стратегій
class VoteShareStrategyFactory {
public:
    static std::unique_ptr<IVoteShareStrategy> createStrategy(ShareStrategyType type) {
        switch (type) {
        case ShareStrategyType::Direct:
            return std::make_unique<DirectShareStrategy>();
        case ShareStrategyType::Ratio:
            return std::make_unique<RatioShareStrategy>();
        case ShareStrategyType::PercentageConverted:
            return std::make_unique<PercentageToShareStrategy>();
        default:
            throw std::invalid_argument("Невідомий тип стратегії обчислення частки голосів.");
        }
    }
};

class VoteShareCalculator {
private:
    std::unique_ptr<IVoteShareStrategy> strategy_;

public:
    explicit VoteShareCalculator(std::unique_ptr<IVoteShareStrategy> strategy)
        : strategy_(std::move(strategy)) {
        if (!strategy_) {
            throw std::invalid_argument("Стратегія не може бути порожньою.");
        }
    }

    void setStrategy(std::unique_ptr<IVoteShareStrategy> strategy) {
        if (!strategy) {
            throw std::invalid_argument("Нова стратегія не може бути порожньою.");
        }
        strategy_ = std::move(strategy);
    }

    std::string currentStrategyName() const {
        return strategy_->name();
    }

    double compute(int candidateVotes, int totalCountedVotes) const {
        return strategy_->calculateShare(candidateVotes, totalCountedVotes);
    }
};