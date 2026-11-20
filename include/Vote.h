#pragma once
#include <string>

struct Vote {
    int id = 0;
    int candidateId = 0; // Зовнішній ключ на Candidate::id
    std::string votingStation;
    int voterAge = 0;
    long long timestamp = 0;
    std::string status = "Зареєстрований";
};

struct VoteStatusHistory {
    int id = 0;
    int voteId = 0; // Зовнішній ключ на Vote::id
    std::string oldStatus;
    std::string newStatus;
    long long timestamp = 0;
};