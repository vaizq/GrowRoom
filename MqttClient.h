//
// Created by vaige on 15.2.2024.
//

#ifndef GROWSTUDIO_MQTTCLIENT_H
#define GROWSTUDIO_MQTTCLIENT_H

#include <string>
#include <functional>
#include <mqtt/async_client.h>


const std::string telemetryTopic("ReservoirController/telemetry");
const std::string responseTopic("ReservoirController/rpc/response");
const std::string requestTopic("ReservoirController/rpc/request");

const int	QOS = 1;
const int	N_RETRY_ATTEMPTS = 5;





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

using MessageHandler = std::function<void(mqtt::const_message_ptr)>;
using ConnectedHandler = std::function<void()>;
using ConnectionLostHandler = std::function<void()>;

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


class MqttClient
{
public:
    MqttClient(const std::string& server, const std::string& clientID)
    : mClient(server, clientID), mCb(mClient, mConnOpts)
    {}

    void connect()
    {
        mConnOpts.set_clean_session(false);
        mClient.set_callback(mCb);

        std::cout << "Connecting to " << mClient.get_server_uri() << std::endl;
        mClient.connect(mConnOpts, nullptr, mCb);
    }

    bool isConnected() const
    {
        return mClient.is_connected();
    }

    void publish(const std::string& topic, const std::string& message)
    {
        mClient.publish(topic, message);
    }

    void subscribe(const std::string& topic)
    {
        mClient.subscribe(topic, QOS);
    }

    void onMessage(MessageHandler cb)
    {
        mCb.onMessage(std::move(cb));
    }

    void onConnected(ConnectedHandler cb)
    {
        mCb.onConnected(std::move(cb));
    }

    void onConnectionLost(ConnectionLostHandler cb)
    {
        mCb.onConnectionLost(std::move(cb));
    }
private:
    mqtt::async_client mClient;
    callback mCb;
    mqtt::connect_options mConnOpts;
};


#endif //GROWSTUDIO_MQTTCLIENT_H
