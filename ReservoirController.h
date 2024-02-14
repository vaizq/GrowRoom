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
#include <mqtt/async_client.h>

const std::string SERVER_ADDRESS("test.mosquitto.org:1883");
const std::string CLIENT_ID("reservoir-controller");
const std::string TOPIC("ReservoirController/#");

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
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

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
        std::cout << "\nConnection success" << std::endl;
        std::cout << "\nSubscribing to topic '" << TOPIC << "'\n"
                  << "\tfor client " << CLIENT_ID
                  << " using QoS" << QOS << "\n"
                  << "\nPress Q<Enter> to quit\n" << std::endl;

        cli_.subscribe(TOPIC, QOS, nullptr, subListener_);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n" << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
            : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
};

class ReservoirController : public App
{
    // Gui stuff
    bool mValveIsOpen{false};
    int mPumpID{};
    float mDoseAmount{};
    float mPH{7.0f};
    float mEC{0.0f};
    std::deque<float> mPHReadings{7.0f, 6.8f, 6.5f, 6.4f, 6.3f, 6.2f};
    std::deque<float> mECReadings{2.0f, 1.8f, 1.5f, 1.4f, 1.3f, 1.2f};
    std::string mLiquidLevel{"empty"};
    mqtt::connect_options mConnOpts;
    mqtt::async_client mClient;
    callback mCb;


    void openValve() {
        std::cout << "openValve()" << std::endl;
    }

    void closeValve() {
        std::cout << "closeValve()" << std::endl;
    }

    void dose(unsigned pumpID, float amount) {
        std::cout << "dose(" << pumpID << ", " << amount << ')' << std::endl;
    }

    void resetDosers() {
        std::cout << "resetDosers()" << std::endl;
    }

    void calibratePHSensor(float phValue) {
        std::cout << "calibratePHSensor(" << phValue << ')' << std::endl;
    }

    void calibrateECSensor(float phValue) {
        std::cout << "calibrateECSensor(" << phValue << ')' << std::endl;
    }

    void handleStatusUpdate(const nlohmann::json& msg) {
        if (msg.contains("ph")) {
            mPHReadings.push_back(msg["ph"]);
        }
        if (msg.contains("ec")) {
            mECReadings.push_back(msg["ec"]);
        }
        if (msg.contains("liquidLevel")) {
            mLiquidLevel = msg["liquidLevel"];
        }
    }

public:
    ReservoirController()
    : mClient(SERVER_ADDRESS, CLIENT_ID), mCb(mClient, mConnOpts)
    {
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

        ImGui::SeparatorText("Status");

        // Status
        ImGui::PlotLines("PH", &mPHReadings.front(), (int)mPHReadings.size());
        ImGui::PlotLines("EC", &mECReadings.front(), (int)mECReadings.size());
        ImGui::Text("LiquidLevel: %s", mLiquidLevel.c_str());
        ImGui::NewLine();
        ImGui::SeparatorText("RPC interface");

        // Valve ON/OFF
        if (ImGui::Button(mValveIsOpen ? "Close valve" : "Open valve")) {
            if (mValveIsOpen) {
                closeValve();
            }
            else {
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
        if(ImGui::Button("calibrate ph-sensor")) {
            calibratePHSensor(mPH);
        }

        // EC calibration
        ImGui::SliderFloat("ec", &mEC, 0.0f, 3.0f);
        ImGui::SameLine();
        if(ImGui::Button("calibrate ec-sensor")) {
            calibrateECSensor(mEC);
        }

        ImGui::End();

        ImGui::ShowDemoWindow();
    }

    void update(Clock::duration dt) override
    {
        onGUI(dt);
    }
};


#endif //GROWSTUDIO_RESERVOIRCONTROLLER_H
