//
// Created by vaige on 14.2.2024.
//

#ifndef GROWSTUDIO_RPCERROR_H
#define GROWSTUDIO_RPCERROR_H

#include <string>
#include <nlohmann/json.hpp>
#include <chrono>


class RpcError
{
    using Clock = std::chrono::steady_clock;
public:
    RpcError() = default;

    RpcError(int code, std::string message, nlohmann::json data = nlohmann::json{}, Clock::duration acuteTime = std::chrono::seconds{3})
    : mCode{code}, mMessage(std::move(message)), mData{std::move(data)}, mAcuteTime{acuteTime}
    {
        mReceiveTime = Clock::now();
    }

    [[nodiscard]] bool isAcute() const
    {
        return Clock::now() - mReceiveTime < mAcuteTime;
    }

    [[nodiscard]] int code() const
    {
        return mCode;
    }

    const std::string& message()
    {
        return mMessage;
    }

private:
    int mCode{};
    std::string mMessage{};
    nlohmann::json mData{};
    Clock::duration mAcuteTime{3};
    Clock::time_point mReceiveTime{};
};


#endif //GROWSTUDIO_RPCERROR_H
