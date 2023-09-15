/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include <string>
#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <vector>
#include <sstream>
#include <mutex>
#include <thread>

#include <aws/crt/Api.h>
#include <aws/crt/JsonObject.h>
#include <aws/crt/UUID.h>
#include <aws/crt/io/HostResolver.h>
#include <aws/iot/MqttClient.h>
#include <aws/iotshadow/ErrorResponse.h>
#include <aws/iotshadow/IotShadowClient.h>
#include <aws/iotshadow/ShadowDeltaUpdatedEvent.h>
#include <aws/iotshadow/ShadowDeltaUpdatedSubscriptionRequest.h>
#include <aws/iotshadow/UpdateShadowRequest.h>
#include <aws/iotshadow/UpdateShadowResponse.h>
#include <aws/iotshadow/UpdateShadowSubscriptionRequest.h>
#include <aws/iotshadow/GetShadowRequest.h>
#include <aws/iotshadow/GetShadowResponse.h>
#include <aws/iotshadow/GetShadowSubscriptionRequest.h>
#include <aws/iotshadow/UpdateShadowSubscriptionRequest.h>

#ifndef COMMANDLINE_UTIL_H
#define COMMANDLINE_UTIL_H
#include "utils/CommandLineUtils.h"
#endif // COMMANDLINE_UTIL_H

#include "ProducerSink.h"
#include "Servo.h"
#include "Logger.h"

LOGGER_TAG("main")

//======================================================================================================================
/// IoT device SDK
using namespace Aws::Crt;
using namespace Aws::Iotshadow;

static const char *SHADOW_VALUE_DEFAULT = "90";

//======================================================================================================================
/// Change shadow value
static void s_changeShadowValue(
    IotShadowClient &client,
    const String &thingName,
    JsonObject &shadowPropertyObject)
{
    ShadowState state;
    JsonObject desired;
    JsonObject reported;
    unsigned int angle;
    unsigned int pulsewidth;

    Map<String, JsonView> shadowPropertyMap = shadowPropertyObject.View().GetAllObjects();

    for (const auto &ele : shadowPropertyMap)
    {
        if (ele.second.AsString() == "null")
        {
            JsonObject nullObject;
            nullObject.AsNull();
            desired.WithObject(ele.first, nullObject);
            reported.WithObject(ele.first, nullObject);
        }
        else if (ele.second.AsString() == "clear_shadow")
        {
            desired.AsNull();
            reported.AsNull();
        }
        else
        {
            desired.WithString(ele.first, ele.second.AsString());
            reported.WithString(ele.first, ele.second.AsString());

            if (ele.first == "pan" || ele.first == "tilt")
            {
                angle = std::stoi(ele.second.AsString().c_str());
                // pan angle range: 0~180 0 left, 90 middle, 180 right
                // tilt angle range: 0~180 0 floor, 90 front, 180 up
                if (angle <= 0)
                {
                    angle = 0;
                }
                else if (angle >= 180)
                {
                    angle = 180;
                }
                pulsewidth = servo::angleToPulsewidth(angle);
                if (ele.first == "pan")
                {
                    servo::panServo(pulsewidth);
                }
                else
                {
                    servo::tiltServo(pulsewidth);
                }
            }
        }
    }

    state.Desired = desired;
    state.Reported = reported;

    UpdateShadowRequest updateShadowRequest;
    Aws::Crt::UUID uuid;
    updateShadowRequest.ClientToken = uuid.ToString();
    updateShadowRequest.ThingName = thingName;
    updateShadowRequest.State = state;

    auto publishCompleted = [thingName](int ioErr)
    {
        if (ioErr != AWS_OP_SUCCESS)
        {
            LOG_FATAL("[DEVICE] Failed to update " << thingName.c_str() << " shadow state: error " << ErrorDebugString(ioErr));
            return;
        }
        LOG_DEBUG("[DEVICE] Successfully updated shadow state for " << thingName.c_str());
    };
    client.PublishUpdateShadow(updateShadowRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, std::move(publishCompleted));
}

//======================================================================================================================
int main(int argc, char **argv)
{
    LOG_CONFIGURE("../kvs_log_configuration");

    /* ------------------------------------------------ */
    // Do the global initialization for the API.
    ApiHandle apiHandle;

    Map<String, String> currentShadowValue;
    std::vector<std::string> vShadowProprty;

    /**
     * cmdData is the arguments/input from the command line placed into a single struct for
     * use in this sample. This handles all of the command line parsing, validating, etc.
     * See the Utils/CommandLineUtils for more information.
     */
    Utils::cmdData cmdData = Utils::parseSampleInputShadow(argc, argv, &apiHandle);

    // Parse shadowPropery and store it to vector
    std::stringstream s_stream(cmdData.input_shadowProperty.c_str()); // create string stream from the string
    while (s_stream.good())
    {
        std::string substr;
        getline(s_stream, substr, ','); // get first string delimited by comma
        vShadowProprty.push_back(substr);
    }

    /* ------------------------------------------------ */
    /// stream to KVS
    int ret;
    // global data
    KVSCustomData kvsdata = {0};
    /* init GStreamer */
    gst_init(&argc, &argv);

    /* build gstreamer pipeline and start */
    ret = gst_init_resources_kvs(&kvsdata, &cmdData);
    if (ret != 0)
    {
        LOG_FATAL("Unable to start pipeline.");
        return 1;
    }

    // Start the appsink process thread
    std::thread thread_bus([&kvsdata]() -> void
                           { code_thread_bus(kvsdata.pipeline, &kvsdata, "RPI"); });

    /* ------------------------------------------------ */
    /// device shadow
    // Create the MQTT builder and populate it with data from cmdData.
    auto clientConfigBuilder =
        Aws::Iot::MqttClientConnectionConfigBuilder(cmdData.input_cert.c_str(), cmdData.input_key.c_str());
    clientConfigBuilder.WithEndpoint(cmdData.input_endpoint);
    if (cmdData.input_ca != "")
    {
        clientConfigBuilder.WithCertificateAuthority(cmdData.input_ca.c_str());
    }
    if (cmdData.input_proxyHost != "")
    {
        Aws::Crt::Http::HttpClientConnectionProxyOptions proxyOptions;
        proxyOptions.HostName = cmdData.input_proxyHost;
        proxyOptions.Port = static_cast<uint16_t>(cmdData.input_proxyPort);
        proxyOptions.AuthType = Aws::Crt::Http::AwsHttpProxyAuthenticationType::None;
        clientConfigBuilder.WithHttpProxyOptions(proxyOptions);
    }
    if (cmdData.input_port != 0)
    {
        clientConfigBuilder.WithPortOverride(static_cast<uint16_t>(cmdData.input_port));
    }

    // Create the MQTT connection from the MQTT builder
    auto clientConfig = clientConfigBuilder.Build();
    if (!clientConfig)
    {
        fprintf(
            stderr,
            "Client Configuration initialization failed with error %s\n",
            Aws::Crt::ErrorDebugString(clientConfig.LastError()));
        LOG_FATAL("[DEVICE] Client Configuration initialization failed with error " << ErrorDebugString(clientConfig.LastError()));
        exit(-1);
    }
    Aws::Iot::MqttClient client = Aws::Iot::MqttClient();
    auto connection = client.NewConnection(clientConfig);
    if (!*connection)
    {
        fprintf(
            stderr,
            "MQTT Connection Creation failed with error %s\n",
            Aws::Crt::ErrorDebugString(connection->LastError()));
        LOG_FATAL("[DEVICE] MQTT Connection Creation failed with error " << ErrorDebugString(connection->LastError()));
        exit(-1);
    }

    /**
     * In a real world application you probably don't want to enforce synchronous behavior
     * but this is a sample console application, so we'll just do that with a condition variable.
     */
    std::promise<bool> connectionCompletedPromise;
    std::promise<void> connectionClosedPromise;

    // Invoked when a MQTT connect has completed or failed
    auto onConnectionCompleted =
        [&](Aws::Crt::Mqtt::MqttConnection &, int errorCode, Aws::Crt::Mqtt::ReturnCode returnCode, bool)
    {
        if (errorCode)
        {
            LOG_FATAL("[DEVICE] Connection failed with error " << ErrorDebugString(errorCode));
            connectionCompletedPromise.set_value(false);
        }
        else
        {
            LOG_INFO("[DEVICE] Connection completed with return code " << returnCode);
            connectionCompletedPromise.set_value(true);
        }
    };

    // Invoked when a disconnect message has completed.
    auto onDisconnect = [&](Aws::Crt::Mqtt::MqttConnection &)
    {
        LOG_INFO("[DEVICE] Disconnect completed");
        connectionClosedPromise.set_value();
    };

    // Assign callbacks
    connection->OnConnectionCompleted = std::move(onConnectionCompleted);
    connection->OnDisconnect = std::move(onDisconnect);

    // Connect
    LOG_INFO("[DEVICE] Connecting...");
    if (!connection->Connect(cmdData.input_clientId.c_str(), true, 0))
    {
        LOG_FATAL("[DEVICE] MQTT Connection failed with error " << ErrorDebugString(connection->LastError()));
        exit(-1);
    }

    if (connectionCompletedPromise.get_future().get())
    {
        Aws::Iotshadow::IotShadowClient shadowClient(connection);

        if (gpioInitialise() < 0)
            return -1;
        gpioSetSignalFunc(SIGINT, servo::stop);

        /********************** Shadow Delta Updates ********************/
        // This section is for when a Shadow document updates/changes, whether it is on the server side or client side.

        std::promise<void> subscribeDeltaCompletedPromise;
        std::promise<void> subscribeDeltaAcceptedCompletedPromise;
        std::promise<void> subscribeDeltaRejectedCompletedPromise;

        auto onDeltaUpdatedSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error subscribing to shadow delta: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            subscribeDeltaCompletedPromise.set_value();
        };

        auto onDeltaUpdatedAcceptedSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error subscribing to shadow delta accepted: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            subscribeDeltaAcceptedCompletedPromise.set_value();
        };

        auto onDeltaUpdatedRejectedSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error subscribing to shadow delta rejected: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            subscribeDeltaRejectedCompletedPromise.set_value();
        };

        auto onDeltaUpdated = [&](ShadowDeltaUpdatedEvent *event, int ioErr)
        {
            if (ioErr)
            {
                LOG_FATAL("[DEVICE] Error processing shadow delta: " << ErrorDebugString(ioErr));
                exit(-1);
            }

            if (event)
            {
                LOG_DEBUG("[DEVICE] Received shadow delta event.");
                if (event->State && vShadowProprty.size() > 0)
                {
                    JsonObject shadowPropertyObject;
                    bool processShadow;
                    // fprintf(stdout, "vShadowProprty.size: %s\n", std::to_string(vShadowProprty.size()).c_str());
                    // fprintf(stdout, "event pan: %s\n", event->State->View().GetString("pan").c_str());
                    // fprintf(stdout, "event tilt: %s\n", event->State->View().GetString("tilt").c_str());

                    for (size_t i = 0; i < vShadowProprty.size(); i++)
                    {
                        if (event->State->View().ValueExists(vShadowProprty.at(i).c_str()))
                        {
                            processShadow = true;
                            JsonView objectView = event->State->View().GetJsonObject(vShadowProprty.at(i).c_str());
                            if (objectView.IsNull())
                            {
                                LOG_DEBUG("[DEVICE] Delta reports that " << vShadowProprty.at(i).c_str() << " was deleted. Resetting defaults...");
                                shadowPropertyObject.WithString(vShadowProprty.at(i).c_str(), SHADOW_VALUE_DEFAULT);
                            }
                            else
                            {
                                LOG_DEBUG("[DEVICE] Delta reports that " << vShadowProprty.at(i).c_str() << " has a desired value of " << event->State->View().GetString(vShadowProprty.at(i).c_str()).c_str() << ", Changing local value...");
                                shadowPropertyObject.WithString(vShadowProprty.at(i).c_str(),
                                                                event->State->View().GetString(vShadowProprty.at(i).c_str()).c_str());
                            }

                            if (event->ClientToken)
                            {
                                LOG_DEBUG("[DEVICE] ClientToken: " << event->ClientToken->c_str());
                            }
                        }
                        else
                        {
                            LOG_DEBUG("[DEVICE] Delta did not report a change in " << vShadowProprty.at(i).c_str() << ".");
                        }
                    }

                    if (processShadow)
                    {
                        s_changeShadowValue(
                            shadowClient,
                            cmdData.input_thingName,
                            shadowPropertyObject);
                    }
                }
                else
                {
                    LOG_INFO("[DEVICE] Delta did not report a change in " << cmdData.input_shadowProperty.c_str() << ".");
                }
            }
        };

        auto onUpdateShadowAccepted = [&](UpdateShadowResponse *response, int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error on subscription: " << ErrorDebugString(ioErr));
                exit(-1);
            }

            if (response->State->Reported)
            {
                for (size_t i = 0; i < vShadowProprty.size(); i++)
                {
                    currentShadowValue[vShadowProprty.at(i).c_str()] = response->State->Reported->View().GetString(vShadowProprty.at(i).c_str());
                }
            }
            else
            {
                for (size_t i = 0; i < vShadowProprty.size(); i++)
                {
                    currentShadowValue[vShadowProprty.at(i).c_str()] = "";
                }
                LOG_DEBUG("[DEVICE] Finished clearing shadow properties");
            }

            LOG_INFO("[DEVICE] Enter Desired state of " << vShadowProprty.at(0).c_str());
        };

        auto onUpdateShadowRejected = [&](ErrorResponse *error, int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error on subscription: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            LOG_INFO("[DEVICE] Update of shadow state failed with message " << error->Message->c_str() << " and code " << *error->Code << ".");
        };

        ShadowDeltaUpdatedSubscriptionRequest shadowDeltaUpdatedRequest;
        shadowDeltaUpdatedRequest.ThingName = cmdData.input_thingName;

        shadowClient.SubscribeToShadowDeltaUpdatedEvents(
            shadowDeltaUpdatedRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onDeltaUpdated, onDeltaUpdatedSubAck);

        UpdateShadowSubscriptionRequest updateShadowSubscriptionRequest;
        updateShadowSubscriptionRequest.ThingName = cmdData.input_thingName;

        shadowClient.SubscribeToUpdateShadowAccepted(
            updateShadowSubscriptionRequest,
            AWS_MQTT_QOS_AT_LEAST_ONCE,
            onUpdateShadowAccepted,
            onDeltaUpdatedAcceptedSubAck);

        shadowClient.SubscribeToUpdateShadowRejected(
            updateShadowSubscriptionRequest,
            AWS_MQTT_QOS_AT_LEAST_ONCE,
            onUpdateShadowRejected,
            onDeltaUpdatedRejectedSubAck);

        subscribeDeltaCompletedPromise.get_future().wait();
        subscribeDeltaAcceptedCompletedPromise.get_future().wait();
        subscribeDeltaRejectedCompletedPromise.get_future().wait();

        /********************** Shadow Value Get ********************/
        // This section is to get the initial value of the Shadow document.

        std::promise<void> subscribeGetShadowAcceptedCompletedPromise;
        std::promise<void> subscribeGetShadowRejectedCompletedPromise;
        std::promise<void> onGetShadowRequestCompletedPromise;
        std::promise<void> gotInitialShadowPromise;

        auto onGetShadowUpdatedAcceptedSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error subscribing to get shadow document accepted: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            subscribeGetShadowAcceptedCompletedPromise.set_value();
        };

        auto onGetShadowUpdatedRejectedSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error subscribing to get shadow document rejected: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            subscribeGetShadowRejectedCompletedPromise.set_value();
        };

        auto onGetShadowRequestSubAck = [&](int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error getting shadow document: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            onGetShadowRequestCompletedPromise.set_value();
        };

        auto onGetShadowAccepted = [&](GetShadowResponse *response, int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error getting shadow value from document: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            if (response)
            {
                LOG_INFO("[DEVICE] Received shadow document. ");
                if (response->State && vShadowProprty.size() > 0)
                {
                    JsonObject shadowPropertyObject;
                    bool processShadow;
                    for (size_t i = 0; i < vShadowProprty.size(); i++)
                    {
                        if (response->State->Reported->View().ValueExists(vShadowProprty.at(i).c_str()))
                        {
                            JsonView objectView = response->State->Reported->View().GetJsonObject(vShadowProprty.at(i).c_str());
                            processShadow = true;
                            if (objectView.IsNull())
                            {
                                LOG_DEBUG("[DEVICE] Shadow contains " << vShadowProprty.at(i).c_str() << " but is null.");
                                currentShadowValue[vShadowProprty.at(i).c_str()] = "";
                            }
                            else
                            {
                                LOG_DEBUG("[DEVICE] Shadow contains " << vShadowProprty.at(i).c_str() << ". Updating local value to " << response->State->Reported->View().GetString(vShadowProprty.at(i).c_str()).c_str() << "...");
                                currentShadowValue[vShadowProprty.at(i).c_str()] = response->State->Reported->View().GetString(vShadowProprty.at(i).c_str());
                            }
                        }
                    }

                    if (processShadow)
                    {
                        s_changeShadowValue(
                            shadowClient,
                            cmdData.input_thingName,
                            shadowPropertyObject);
                    }
                }
                else
                {
                    LOG_INFO("[DEVICE] Shadow currently does not contain " << cmdData.input_shadowProperty.c_str() << ".");
                }
                gotInitialShadowPromise.set_value();
            }
        };

        auto onGetShadowRejected = [&](ErrorResponse *error, int ioErr)
        {
            if (ioErr != AWS_OP_SUCCESS)
            {
                LOG_FATAL("[DEVICE] Error on getting shadow document: " << ErrorDebugString(ioErr));
                exit(-1);
            }
            LOG_INFO("[DEVICE] Getting shadow document failed with message " << error->Message->c_str() << " and code " << *error->Code);
            gotInitialShadowPromise.set_value();
        };

        GetShadowSubscriptionRequest shadowSubscriptionRequest;
        shadowSubscriptionRequest.ThingName = cmdData.input_thingName;

        shadowClient.SubscribeToGetShadowAccepted(
            shadowSubscriptionRequest,
            AWS_MQTT_QOS_AT_LEAST_ONCE,
            onGetShadowAccepted,
            onGetShadowUpdatedAcceptedSubAck);

        shadowClient.SubscribeToGetShadowRejected(
            shadowSubscriptionRequest,
            AWS_MQTT_QOS_AT_LEAST_ONCE,
            onGetShadowRejected,
            onGetShadowUpdatedRejectedSubAck);

        subscribeGetShadowAcceptedCompletedPromise.get_future().wait();
        subscribeGetShadowRejectedCompletedPromise.get_future().wait();

        GetShadowRequest shadowGetRequest;
        shadowGetRequest.ThingName = cmdData.input_thingName;

        // Get the current shadow document so we start with the correct value
        shadowClient.PublishGetShadow(shadowGetRequest, AWS_MQTT_QOS_AT_LEAST_ONCE, onGetShadowRequestSubAck);

        onGetShadowRequestCompletedPromise.get_future().wait();
        gotInitialShadowPromise.get_future().wait();

        /********************** Shadow change value input loop ********************/
        /**
         * This section is to getting user input and changing the shadow value passed to that input.
         * If in CI, then input is automatically passed
         */

        LOG_INFO("[DEVICE] Enter Desired state of " << vShadowProprty.at(0).c_str());
        while (true)
        {
            String input;
            JsonObject shadowPropertyObject;
            std::cin >> input;

            if (input == "exit" || input == "quit")
            {
                fprintf(stdout, "Exiting...");
                break;
            }

            if (input == currentShadowValue[vShadowProprty.at(0).c_str()])
            {
                LOG_INFO("[DEVICE] Shadow is already set to " << input.c_str());
                LOG_INFO("[DEVICE] Enter Desired state of " << vShadowProprty.at(0).c_str());
            }
            else
            {
                LOG_INFO("[DEVICE] input is " << input.c_str() << " property is " << vShadowProprty.at(0).c_str());
                shadowPropertyObject.WithString(vShadowProprty.at(0).c_str(), input);

                s_changeShadowValue(
                    shadowClient,
                    cmdData.input_thingName,
                    shadowPropertyObject);
            }
        }
    }

    // Disconnect
    if (connection->Disconnect())
    {
        connectionClosedPromise.get_future().wait();
    }
    gpioTerminate();

    /* ------------------------------------------------ */
    // Wait for threads
    // thread_bus.join();

    /* free gstreamer resources */
    gst_free_resources(kvsdata.pipeline);

    return 0;
}