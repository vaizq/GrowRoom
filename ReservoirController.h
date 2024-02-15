//
// Created by vaige on 14.2.2024.
//

#ifndef GROWSTUDIO_RESERVOIRCONTROLLER_H
#define GROWSTUDIO_RESERVOIRCONTROLLER_H

#include "App.h"
#include "imgui.h"
#include <deque>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <functional>
#include "MqttClient.h"
#include "RpcError.h"
#include <map>


const std::string SERVER_ADDRESS("test.mosquitto.org:1883");
const std::string CLIENT_ID("reservoir-controller");


class ReservoirController : public App
{
    using ResponseHandler = std::function<void(const nlohmann::json& response)>;
    // Gui stuff
    bool mValveIsOpen{false};
    int mPumpID{};
    float mDoseAmount{};
    float mCalibrationPH{7.0f};
    float mCalibrationEC{0.0f};
    std::deque<float> mPHReadings{};
    std::deque<float> mECReadings{};
    std::string mLiquidLevel{"empty"};
    std::deque<mqtt::const_message_ptr> mMessage;
    static constexpr std::size_t readingsMax{100};
    RpcError mRpcError{};
    int mDosersCount{-1};
    std::map<int, ResponseHandler> mResponseHandlers;
    MqttClient mClient;


    void openValve() {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "openValve"}
        }.dump());
    }

    void closeValve() {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "closeValve"}
                        }.dump());
    }

    void dose(unsigned doserID, float amount) {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "dose"},
                                {"params",
                                     nlohmann::json{
                                            {"doserID", doserID},
                                            {"amount",  amount}
                                    }
                                }
                        }.dump());
    }

    void resetDosers() {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "resetDosers"}
                        }.dump());
    }

    void calibratePHSensor(float ph) {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "calibratePHSensor"},
                                {"params",
                                    nlohmann::json{
                                        {"phValue", ph}
                                    }
                                }
                        }.dump());
    }

    void calibrateECSensor(float ec) {
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", 0},
                                {"method", "calibrateECSensor"},
                                {"params",
                                 nlohmann::json{
                                         {"ecValue", ec}
                                 }
                                }
                        }.dump());
    }

    void getDosersCount() {
        int id = 420;
        mClient.publish(requestTopic,
                        nlohmann::json{
                                {"jsonrpc", "2.0"},
                                {"id", id},
                                {"method", "dosersCount"},
                        }.dump());

        onResponse(id, [this](const nlohmann::json& response) {
            if (response.contains("result")) {
                mDosersCount = response["result"];
            }
        });
    }

    void onResponse(int id, ResponseHandler handler)
    {
        mResponseHandlers[id] = std::move(handler);
    }

    void handleStatusUpdate(const nlohmann::json& msg) {
        if (msg.contains("ph")) {
            mPHReadings.push_back(msg["ph"]);
            if (mPHReadings.size() > readingsMax) {
                mPHReadings.pop_front();
            }
        }
        if (msg.contains("ec")) {
            mECReadings.push_back(msg["ec"]);
        }
        if (msg.contains("liquidLevel")) {
            mLiquidLevel = msg["liquidLevel"];
        }
    }

    void handleResponse(const nlohmann::json& response) {
        if (!response.contains("id")) {
            std::cerr << "Response does not contain id" << std::endl;
            return;
        }

        if (mResponseHandlers.contains(response["id"])) {
            mResponseHandlers[response["id"]](response);
        }

        if (response.contains("error")) {
            std::cout << "Error!" << std::endl;
            mRpcError = RpcError{response["error"].value("code", -1), response["error"].value("message", "No message")};
        }
    }

    void handleMessages()
    {
        while (!mMessage.empty())
        {
            auto msg = mMessage.front();

            if (msg->get_topic() == telemetryTopic) {
                handleStatusUpdate(nlohmann::json::parse(msg->get_payload()));
            }
            else if (msg->get_topic() == responseTopic) {
                handleResponse(nlohmann::json::parse(msg->get_payload()));
            }

            mMessage.pop_front();
        }
    }

public:
    ReservoirController()
    : mClient(SERVER_ADDRESS, CLIENT_ID)
    {
        mClient.onMessage([this](mqtt::const_message_ptr msg) {
            mMessage.push_back(msg);
        });

        mClient.connect();
    }

    void onGUI(Clock::duration dt)
    {
        ImGui::Begin("ReservoirController");

        if (mClient.isConnected()) {

            ImGui::SeparatorText("Status");

            // Status
            if (!mPHReadings.empty()) {
                std::vector<float> readings{mPHReadings.begin(), mPHReadings.end()};
                ImGui::PlotLines(fmt::format("PH [{:.2f}]", mPHReadings.back()).c_str(), &readings.front(), static_cast<int>(readings.size()));
            }

            if (!mECReadings.empty()) {
                std::vector<float> readings{mECReadings.begin(), mECReadings.end()};
                ImGui::PlotLines(fmt::format("EC [{:.2f}]", mECReadings.back()).c_str(), &readings.front(), static_cast<int>(readings.size()));
            }

            ImGui::Text("LiquidLevel: %s", mLiquidLevel.c_str());
            ImGui::NewLine();
            ImGui::SeparatorText("RPC interface");

            // Valve ON/OFF
            if (ImGui::Button(mValveIsOpen ? "Close valve" : "Open valve")) {
                if (mValveIsOpen) {
                    closeValve();
                } else {
                    openValve();
                }

                mValveIsOpen = !mValveIsOpen;
            }

            // Dosing
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("pumpID", &mPumpID);
            ImGui::SameLine();
            ImGui::SliderFloat("amount", &mDoseAmount, 0.0f, 100.0f);
            ImGui::SameLine();
            if (ImGui::Button("dose")) {
                dose(static_cast<unsigned>(mPumpID), mDoseAmount);
            }

            if (ImGui::Button("reset dosers")) {
                resetDosers();
            }

            // PH calibration
            ImGui::SliderFloat("ph", &mCalibrationPH, 0.0f, 14.0f);
            ImGui::SameLine();
            if (ImGui::Button("calibrate ph-sensor")) {
                ImGui::OpenPopup("Calibrate PH");
            }

            if (ImGui::BeginPopupModal("Calibrate PH")) {
                ImGui::Text("Is your PH probe in %.2f calibration solution?", mCalibrationPH);
                if (ImGui::Button("Yes")) {
                    ImGui::CloseCurrentPopup();
                    calibratePHSensor(mCalibrationPH);
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // EC calibration
            ImGui::SliderFloat("ec", &mCalibrationEC, 0.0f, 3.0f);
            ImGui::SameLine();
            if (ImGui::Button("calibrate ec-sensor")) {
                ImGui::OpenPopup("Calibrate EC");
            }

            if (ImGui::BeginPopupModal("Calibrate EC")) {
                ImGui::Text("Is your EC probe in %.2f calibration solution?", mCalibrationEC);
                if (ImGui::Button("Yes")) {
                    ImGui::CloseCurrentPopup();
                    calibrateECSensor(mCalibrationEC);
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // Show dosers count
            ImGui::Text("Dosers count: %s", (mDosersCount == -1) ? "unknown" : std::to_string(mDosersCount).c_str());
            ImGui::SameLine();
            if (ImGui::Button("get dosers count")) {
                getDosersCount();
            }

            // Error display
            if (mRpcError.isAcute()) {
                ImGui::Text("Error { code: %d, message: %s }", mRpcError.code(), mRpcError.message().c_str());
            }
        }
        else {
            ImGui::Text("Not Connected");
        }

        ImGui::End();

        ImGui::ShowDemoWindow();
    }

    void update(Clock::duration dt) override
    {
        handleMessages();
        onGUI(dt);
    }
};


#endif //GROWSTUDIO_RESERVOIRCONTROLLER_H