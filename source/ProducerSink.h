/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include <string>
#include <iostream>
#include <gst/gst.h>

#ifndef COMMANDLINE_UTIL_H
#define COMMANDLINE_UTIL_H
#include "utils/CommandLineUtils.h"
#endif // COMMANDLINE_UTIL_H

/// Structure to contain all our information, so we can pass it to callbacks
typedef struct KVSCustomData
{
    // GstElement *pipeline, *source, *capsfilter, *videoconvert, *videoscale, *overlay, *sink;
    GstElement *pipeline, *source, *capsfilter, *overlay, *encoder, *encodercapsfilter, *parser, *kvssink;

    GstBus *bus;
    GMainLoop *main_loop; /* GLib's Main Loop */
} KVSCustomData;

/// init gstreamer
int gst_init_resources_kvs(KVSCustomData *kvsdata, Utils::cmdData *cmdData);

/// Run the message loop for one bus
void code_thread_bus(GstElement *pipeline, KVSCustomData *data, const std::string &prefix);

/// clean up GStream resources
void gst_free_resources(GstElement *pipeline);
