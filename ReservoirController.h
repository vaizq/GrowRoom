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
#include <mqtt/async_client.h>
#include "RpcError.h"
#include <map>

const std::string SERVER_ADDRESS("test.mosquitto.org:1883");
const std::string CLIENT_ID("reservoir-controller");
const std::string telemetryTopic("ReservoirController/telemetry");
const std::string responseTopic("ReservoirController/rpc/response");
const std::string requestTopic("ReservoirController/rpc/request");

const int	QOS = 1;
const int	N_RETRY_ATTEMPTS = 5;

class ReservoirController;

/////////////////////////////////////////////////////////////////////////////

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string& name) : name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
                 public virtual mqtt::iaction_listener

{
public:
    using MessageHandler = std::function<void(mqtt::const_message_ptr)>;
    using ConnectedHandler = std::function<void()>;
    using ConnectionLostHandler = std::function<void()>;

private:
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;
    MessageHandler mMessageHandler{[](auto){}};
    ConnectedHandler mConnectedHandler{[](){}};
    ConnectionLostHandler mConnectionLostHandler{[](){}};

    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token& tok) override {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token& tok) override {}

    // (Re)connection success
    void connected(const std::string& cause) override {
        mConnectedHandler();
        cli_.subscribe(telemetryTopic, QOS, nullptr, subListener_);
        cli_.subscribe(responseTopic, QOS, nullptr, subListener_);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override {
        mConnectionLostHandler();

        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override {
        mMessageHandler(msg);
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
            : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}

    void onMessage(MessageHandler h) {
        mMessageHandler = std::move(h);
    }

    void onConnected(ConnectedHandler h) {
        mConnectedHandler = std::move(h);
    }

    void onConnectionLost(ConnectionLostHandler h) {
        mConnectionLostHandler = std::move(h);
    }
};

class ReservoirController : public App
{
    using ResponseHandler = std::function<void(const nlohmann::json& response)>;
    // Gui stuff
    bool mValveIsOpen{false};
    int mPumpID{};
    float mDoseAmount{};
    float mPH{7.0f};
    float mEC{0.0f};
    std::deque<float> mPHReadings{};
    std::deque<float> mECReadings{};
    std::string mLiquidLevel{"empty"};
    mqtt::connect_options mConnOpts;
    mqtt::async_client mClient;
    callback mCb;
    std::deque<mqtt::const_message_ptr> mMessage;
    bool mConnected{false};
    static constexpr std::size_t maxReadings{100};
    RpcError mRpcError{};
    int mDosersCount{-1};
    std::map<int, ResponseHandler> mResponseHandlers;



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
            if (mPHReadings.size() > maxReadings) {
                mPHReadings.pop_front();
            }
        }
        if (msg.contains("ec")) {
            mECReadings.push_back(msg["ec"]);
            if (mECReadings.size() > maxReadings) {
                mECReadings.pop_front();
            }
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

        try {
            if (mResponseHandlers.contains(response["id"])) {
                mResponseHandlers[response["id"]](response);
            }
            if (response.contains("error")) {
                std::cout << "Error!" << std::endl;
                mRpcError = RpcError{response["error"].value("code", -1), response["error"].value("message", "No message")};
            }
        }
        catch(const std::exception& e) {
            std::cerr << "unable to handle response" << std::endl;
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
    : mClient(SERVER_ADDRESS, CLIENT_ID), mCb(mClient, mConnOpts)
    {
        mCb.onMessage([this](mqtt::const_message_ptr msg) {
            mMessage.push_back(msg);
        });

        mCb.onConnected([this]() {
            mConnected = true;
        });

        mCb.onConnectionLost([this]() {
            mConnected = false;
        });


        mConnOpts.set_clean_session(false);
        mClient.set_callback(mCb);

        // Start the connection.
        // When completed, the callback will subscribe to topic.
        std::cout << "Connecting to the MQTT server..." << std::flush;
        mClient.connect(mConnOpts, nullptr, mCb);
    }

    void onGUI(Clock::duration dt)
    {
        ImGui::Begin("ReservoirController");

        if (mConnected) {

            ImGui::SeparatorText("Status");

            // Status
            ImGui::PlotLines("PH", &mPHReadings.front(), (int) mPHReadings.size());
            ImGui::SameLine();
            if (!mPHReadings.empty())
                ImGui::Value(": ", mPHReadings.back());
            ImGui::PlotLines("EC", &mECReadings.front(), (int) mECReadings.size());
            ImGui::SameLine();
            if (!mECReadings.empty())
                ImGui::Value(":", mECReadings.back());
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
            ImGui::SliderFloat("ph", &mPH, 0.0f, 14.0f);
            ImGui::SameLine();
            if (ImGui::Button("calibrate ph-sensor")) {
                ImGui::OpenPopup("Calibrate PH");
            }

            if (ImGui::BeginPopupModal("Calibrate PH")) {
                ImGui::Text("Is your PH probe in %.2f calibration solution?", mPH);
                if (ImGui::Button("Yes")) {
                    ImGui::CloseCurrentPopup();
                    calibratePHSensor(mPH);
                }
                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // EC calibration
            ImGui::SliderFloat("ec", &mEC, 0.0f, 3.0f);
            ImGui::SameLine();
            if (ImGui::Button("calibrate ec-sensor")) {
                ImGui::OpenPopup("Calibrate EC");
            }

            if (ImGui::BeginPopupModal("Calibrate EC")) {
                ImGui::Text("Is your EC probe in %.2f calibration solution?", mEC);
                if (ImGui::Button("Yes")) {
                    ImGui::CloseCurrentPopup();
                    calibrateECSensor(mEC);
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
