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

#ifndef GST_H
#define GST_H
#include <gst/gst.h>
#endif // GST_H

#ifndef COMMANDLINE_UTIL_H
#define COMMANDLINE_UTIL_H
#include "utils/CommandLineUtils.h"
#endif // COMMANDLINE_UTIL_H

#include "DeviceManager.h"
#include "Logger.h"
#include "WebRtcCommon.h"

LOGGER_TAG("main")

//======================================================================================================================
int main(int argc, char **argv)
{
    LOG_CONFIGURE("../kvs_log_configuration");

    /* ------------------------------------------------ */
    // Do the global initialization for the Device SDK API.
    Aws::Crt::ApiHandle apiHandle;

    /**
     * cmdData is the arguments/input from the command line placed into a single struct for
     * use in this sample. This handles all of the command line parsing, validating, etc.
     * See the Utils/CommandLineUtils for more information.
     */
    Utils::cmdData cmdData = Utils::parseSampleInputShadow(argc, argv, &apiHandle);

    /* ------------------------------------------------ */
    /// device shadow
    std::thread thread_shadow([&cmdData]
                              { mamageDeviceShadow(cmdData); });
    LOG_INFO("[DEVICE] thread_shadow started");
    // /* ------------------------------------------------ */

    // for KVS WebRTC
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    IotCoreCredential pIotCoreCredential;

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

    signal(SIGINT, sigintHandler);

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    // To make the IAM policy for assume role for iot:CredentialProvider more secure,
    // it is using restrcting you to use the thing name as a channel name
    // "Resource": "arn:aws:kinesisvideo:*:*:channel/${credentials-iot:ThingName}/*"
    pChannelName = (char *)cmdData.input_thingName.c_str();
#else
    pChannelName = (char *)cmdData.input_channelName.c_str();
#endif

    // IoT Credential
    pIotCoreCredential.pIotCoreCredentialEndPoint = (char *)cmdData.input_credentialEndpoint.c_str();
    pIotCoreCredential.pIotCoreCaCertPath = (char *)cmdData.input_ca.c_str();
    pIotCoreCredential.pIotCoreCert = (char *)cmdData.input_cert.c_str();
    pIotCoreCredential.pIotCorePrivateKey = (char *)cmdData.input_key.c_str();
    pIotCoreCredential.pIotCoreRoleAlias = (char *)cmdData.input_roleAlias.c_str();
    pIotCoreCredential.pKVSRegion = (char *)cmdData.input_kvsRegion.c_str();

    CHK_STATUS(createSampleConfiguration(pChannelName, &pIotCoreCredential, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    pSampleConfiguration->videoSource = sendGstreamerAudioVideo;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
    pSampleConfiguration->receiveAudioVideoSource = receiveGstreamerAudioVideo;
    pSampleConfiguration->onDataChannel = onDataChannel;
    pSampleConfiguration->customData = (UINT64)pSampleConfiguration;
    pSampleConfiguration->srcType = DEVICE_SOURCE; // Default to device source (autovideosrc and autoaudiosrc)
    /* Initialize GStreamer */
    gst_init(&argc, &argv);
    LOG_INFO("[KVS Gstreamer Master] Finished initializing GStreamer and handlers");

    LOG_INFO("[KVS Gstreamer Master] KVS region " << pSampleConfiguration->channelInfo.pRegion);

    if (STRCMP((char *)cmdData.input_mediaType.c_str(), "video-only") == 0)
    {
        pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
        LOG_INFO("[KVS Gstreamer Master] Streaming video only");
    }
    else if (STRCMP((char *)cmdData.input_mediaType.c_str(), "audio-video") == 0)
    {
        pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
        LOG_INFO("[KVS Gstreamer Master] Streaming audio and video");
    }
    else
    {
        LOG_INFO("[KVS Gstreamer Master] Unrecognized streaming type. Default to video-only");
    }

    if (STRCMP((char *)cmdData.input_mediaSourceType.c_str(), "testsrc") == 0)
    {
        LOG_INFO("[KVS GStreamer Master] Using test source in GStreamer");
        pSampleConfiguration->srcType = TEST_SOURCE;
    }
    else if (STRCMP((char *)cmdData.input_mediaSourceType.c_str(), "devicesrc") == 0)
    {
        LOG_INFO("[KVS GStreamer Master] Using device source in GStreamer");
        pSampleConfiguration->srcType = DEVICE_SOURCE;
    }
    else if (STRCMP((char *)cmdData.input_mediaSourceType.c_str(), "rpisrc") == 0)
    {
        LOG_INFO("[KVS GStreamer Master] Using device source in GStreamer");
        pSampleConfiguration->srcType = RPI_SOURCE;
    }
    else if (STRCMP((char *)cmdData.input_mediaSourceType.c_str(), "rtspsrc") == 0)
    {
        LOG_INFO("[KVS GStreamer Master] Using RTSP source in GStreamer");
        if (STRCMP((char *)cmdData.input_mediaSourceType.c_str(), "") == 0)
        {
            printf("[KVS GStreamer Master] No RTSP source URI included. Defaulting to device source");
            printf("[KVS GStreamer Master] Usage: ./kvsWebrtcClientMasterGstSample <channel name> audio-video rtspsrc rtsp://<rtsp uri>\n"
                   "or ./kvsWebrtcClientMasterGstSample <channel name> video-only rtspsrc <rtsp://<rtsp uri>");
            pSampleConfiguration->srcType = DEVICE_SOURCE;
        }
        else
        {
            pSampleConfiguration->srcType = RTSP_SOURCE;
            pSampleConfiguration->rtspUri = (char *)cmdData.input_rtspUri.c_str();
        }
    }
    else
    {
        LOG_INFO("[KVS Gstreamer Master] Unrecognized source type. Defaulting to device source in GStreamer");
    }

    // Initalize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());
    LOG_INFO("[KVS GStreamer Master] KVS WebRTC initialization completed successfully");

    CHK_STATUS(initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID));
    LOG_INFO("[KVS Gstreamer Master] Channel " << pChannelName << " set up done");

    /* ------------------------------------------------ */
    // Checking for termination
    // std::thread thread_kvs([&pSampleConfiguration]
    //                        { sessionCleanupWait(pSampleConfiguration); });
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    LOG_INFO("[KVS GStreamer Master] Streaming session terminated");
    /* ------------------------------------------------ */

CleanUp:
    // KVS WebRTC SDK C
    cleanUpKVSResources(pSampleConfiguration);
}