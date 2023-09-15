/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#pragma once

#include <string>
#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>

// #include <aws/crt/Api.h>
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

using namespace Aws::Crt;
using namespace Aws::Iotshadow;

static const char *SHADOW_VALUE_DEFAULT = "90";

/// Change shadow value
static void s_changeShadowValue(IotShadowClient &client,
                                const String &thingName,
                                JsonObject &shadowPropertyObject);

/// device shadow
int mamageDeviceShadow(Utils::cmdData &cmdData);