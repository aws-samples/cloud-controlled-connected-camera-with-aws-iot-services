/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include "ProducerSink.h"
#include "Logger.h"

LOGGER_TAG("videosink")

/// Process a single bus message, log messages, exit on error, return false on eos
static bool bus_process_msg(GstElement *pipeline, GstMessage *msg, const std::string &prefix)
{
    using namespace std;

    GstMessageType mType = GST_MESSAGE_TYPE(msg);
    LOG_DEBUG("BUS THREAD FINISHED : " << prefix);
    switch (mType)
    {
    case (GST_MESSAGE_ERROR):
        // Parse error and exit program, hard exit
        GError *err;
        gchar *dbg;
        gst_message_parse_error(msg, &err, &dbg);
        LOG_DEBUG("ERR = " << err->message << " FROM " << GST_OBJECT_NAME(msg->src));
        LOG_DEBUG("DBG = " << dbg);
        g_clear_error(&err);
        g_free(dbg);
        exit(1);
    case (GST_MESSAGE_EOS):
        // Soft exit on EOS
        LOG_DEBUG("EOS !");
        return false;
    case (GST_MESSAGE_STATE_CHANGED):
        // Parse state change, print extra info for pipeline only
        LOG_DEBUG("State changed !");
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
        {
            GstState sOld, sNew, sPenging;
            gst_message_parse_state_changed(msg, &sOld, &sNew, &sPenging);
            LOG_DEBUG("Pipeline changed from " << gst_element_state_get_name(sOld) << " to " << gst_element_state_get_name(sNew));
        }
        break;
    case (GST_MESSAGE_STEP_START):
        LOG_DEBUG("STEP START !");
        break;
    case (GST_MESSAGE_STREAM_STATUS):
        LOG_DEBUG("STREAM STATUS !");
        break;
    case (GST_MESSAGE_ELEMENT):
        LOG_DEBUG("MESSAGE ELEMENT !");
        break;

    default:
        break;
    }
    return true;
}

/// Run the message loop for one bus
void code_thread_bus(GstElement *pipeline, KVSCustomData *data, const std::string &prefix)
{
    GstBus *bus = gst_element_get_bus(pipeline);

    int res;
    while (true)
    {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        res = bus_process_msg(pipeline, msg, prefix);
        gst_message_unref(msg);
        if (!res)
            break;
    }
    gst_object_unref(bus);

    LOG_DEBUG("BUS THREAD FINISHED : " << prefix);
}

/// init gstreamer
int gst_init_resources_kvs(KVSCustomData *kvsdata, Utils::cmdData *cmdData)
{
    LOG_INFO("Entering gst_init_resources_kvs... ");

    GstStateChangeReturn ret;
    GstPlugin *plugin = gst_plugin_load_file("/home/pi/sdk-workspace/amazon-kinesis-video-streams-producer-sdk-cpp-build/libgstkvssink.so", NULL);

    if (plugin == nullptr)
    {
        plugin = gst_plugin_load_file("/usr/local/lib/libgstkvssink.so", NULL);
        if (plugin == nullptr)
        {
            LOG_FATAL("kvssink plugin failed to load!");
            exit(1);
        }
    }

    GstRegistry *registry;
    registry = gst_registry_get();
    gst_registry_add_plugin(registry, plugin);

    LOG_DEBUG("Finished loading kvssink plugin... ");

    // Create elements
    kvsdata->source = gst_element_factory_make("libcamerasrc", "mysource");
    kvsdata->capsfilter = gst_element_factory_make("capsfilter", "mycapsfilter");
    kvsdata->overlay = gst_element_factory_make("clockoverlay", "myoverlay");
    kvsdata->encoder = gst_element_factory_make("v4l2h264enc", "myencoder");
    kvsdata->encodercapsfilter = gst_element_factory_make("capsfilter", "myencodercapsfilter");
    kvsdata->parser = gst_element_factory_make("h264parse", "myparser");
    kvsdata->kvssink = gst_element_factory_make("kvssink", "mykvssink");
    LOG_DEBUG("Finished creating elements... ");
    kvsdata->pipeline = gst_pipeline_new("mypipeline");
    LOG_DEBUG("Finished empty pipeline ... ");
    if (!kvsdata->pipeline || !kvsdata->source || !kvsdata->capsfilter || !kvsdata->overlay || !kvsdata->encoder || !kvsdata->encodercapsfilter || !kvsdata->parser || !kvsdata->kvssink)
    {
        LOG_FATAL("Not all elements could be created.\n");
        return -1;
    }

    // Set caps for capsfilter
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "width", G_TYPE_INT, 1280,
                                        "height", G_TYPE_INT, 720,
                                        "format", G_TYPE_STRING, "NV12",
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        "interlace-mode", G_TYPE_STRING, "progressive",
                                        "colorimetry", G_TYPE_STRING, "bt709",
                                        NULL);
    g_object_set(G_OBJECT(kvsdata->capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);
    LOG_DEBUG("Created source filter...");

    g_object_set(G_OBJECT(kvsdata->overlay), "time-format", "%d/%m/%y %H:%M:%S", NULL);

    /* configure encoder */
    // gst-inspect-1.0 v4l2h264enc
    GstStructure *extrastruct = gst_structure_new("controls",
                                                  "h264_profile", G_TYPE_INT, 4,
                                                  "video_bitrate", G_TYPE_INT, 620000,
                                                  NULL);
    g_object_set(G_OBJECT(kvsdata->encoder), "extra-controls", extrastruct, NULL);
    gst_structure_free(extrastruct);
    LOG_DEBUG("Created encoder...");

    // Set caps for encodercapsfilter
    GstCaps *h264_caps = gst_caps_new_simple("video/x-h264",
                                             "profile", G_TYPE_STRING, "high",
                                             "level", G_TYPE_STRING, "4",
                                             NULL);
    g_object_set(G_OBJECT(kvsdata->encodercapsfilter), "caps", h264_caps, NULL);
    gst_caps_unref(h264_caps);
    LOG_DEBUG("Created encoder filter...");

    // kvssink
    LOG_DEBUG("Setting IOT Credentials");
    GstStructure *iot_credentials = gst_structure_new("iot-certificate",
                                                      "iot-thing-name", G_TYPE_STRING, cmdData->input_thingName.c_str(),
                                                      "endpoint", G_TYPE_STRING, cmdData->input_credentialEndpoint.c_str(),
                                                      "cert-path", G_TYPE_STRING, cmdData->input_cert.c_str(),
                                                      "key-path", G_TYPE_STRING, cmdData->input_key.c_str(),
                                                      "ca-path", G_TYPE_STRING, cmdData->input_ca.c_str(),
                                                      "role-aliases", G_TYPE_STRING, cmdData->input_roleAlias.c_str(),
                                                      NULL);
    g_object_set(G_OBJECT(kvsdata->kvssink), "iot-certificate", iot_credentials, NULL);
    gst_structure_free(iot_credentials);
    g_object_set(G_OBJECT(kvsdata->kvssink),
                 "stream-name", cmdData->input_thingName.c_str(),
                 "storage-size", 1000,
                 "aws-region", cmdData->input_kvsRegion.c_str(),
                 "retention-period", 2,
                 NULL);
    LOG_DEBUG("About to build pipeline...");

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(kvsdata->pipeline), kvsdata->source, kvsdata->capsfilter, kvsdata->overlay, kvsdata->encoder, kvsdata->encodercapsfilter, kvsdata->parser, kvsdata->kvssink, NULL);
    // Link elements
    if (!gst_element_link_many(kvsdata->source, kvsdata->capsfilter, kvsdata->overlay, kvsdata->encoder, kvsdata->encodercapsfilter, kvsdata->parser, kvsdata->kvssink, NULL))
    {
        LOG_FATAL("Elements could not be linked.");
        gst_object_unref(kvsdata->pipeline);
        return -1;
    }
    // Start playing
    ret = gst_element_set_state(kvsdata->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        LOG_FATAL("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(kvsdata->pipeline);
        return 1;
    }

    return 0;
}

/// clean up GStream resources
void gst_free_resources(GstElement *pipeline)
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    LOG_DEBUG("GST resources successfully freed...");
}