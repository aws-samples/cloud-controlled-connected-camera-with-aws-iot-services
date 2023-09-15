/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#include "CommandLineUtils.h"
#include <aws/crt/Api.h>
#include <aws/crt/Types.h>
#include <aws/crt/UUID.h>
#include <fstream>
#include <iostream>

namespace Utils
{
    // The command names for the samples
    static const char *m_cmd_endpoint = "endpoint";
    static const char *m_cmd_ca_file = "ca_file";
    static const char *m_cmd_cert_file = "cert";
    static const char *m_cmd_key_file = "key";
    static const char *m_cmd_credential_endpoint = "credential_endpoint";
    static const char *m_cmd_role_alias = "role_alias";
    static const char *m_cmd_message = "message";
    static const char *m_cmd_topic = "topic";
    static const char *m_cmd_help = "help";
    static const char *m_cmd_client_id = "client_id";
    static const char *m_cmd_thing_name = "thing_name";
    static const char *m_cmd_is_ci = "is_ci";
    static const char *m_cmd_shadow_property = "shadow_property";
    // static const char *m_cmd_channel_name = "channel_name";
    static const char *m_cmd_kvs_region = "kvs_region";
    static const char *m_cmd_media_type = "media_type";
    static const char *m_cmd_media_source_type = "media_source_type";
    static const char *m_cmd_rtsp_uri = "rspt_uri";
    static const char *m_cmd_verbosity = "verbosity";
    static const char *m_cmd_log_file = "log_file";

    CommandLineUtils::CommandLineUtils()
    {
        // Automatically register the help command
        RegisterCommand(m_cmd_help, "", "Prints this message");
    }

    void CommandLineUtils::RegisterProgramName(Aws::Crt::String newProgramName)
    {
        m_programName = std::move(newProgramName);
    }

    void CommandLineUtils::RegisterCommand(CommandLineOption option)
    {
        if (m_registeredCommands.count(option.m_commandName))
        {
            fprintf(stdout, "Cannot register command: %s: Command already registered!", option.m_commandName.c_str());
            return;
        }
        m_registeredCommands.insert({option.m_commandName, option});
    }

    void CommandLineUtils::RegisterCommand(
        Aws::Crt::String commandName,
        Aws::Crt::String exampleInput,
        Aws::Crt::String helpOutput)
    {
        RegisterCommand(CommandLineOption(commandName, exampleInput, helpOutput));
    }

    void CommandLineUtils::RemoveCommand(Aws::Crt::String commandName)
    {
        if (m_registeredCommands.count(commandName))
        {
            m_registeredCommands.erase(commandName);
        }
    }

    void CommandLineUtils::UpdateCommandHelp(Aws::Crt::String commandName, Aws::Crt::String newCommandHelp)
    {
        if (m_registeredCommands.count(commandName))
        {
            m_registeredCommands.at(commandName).m_helpOutput = std::move(newCommandHelp);
        }
    }

    void CommandLineUtils::SendArguments(const char **argv, const char **argc)
    {
        if (m_beginPosition != nullptr || m_endPosition != nullptr)
        {
            fprintf(stdout, "Arguments already sent!");
            return;
        }
        m_beginPosition = argv;
        m_endPosition = argc;

        // Automatically check and print the help message if the help command is present
        if (HasCommand(m_cmd_help))
        {
            PrintHelp();
            exit(-1);
        }
    }

    bool CommandLineUtils::HasCommand(Aws::Crt::String command)
    {
        return std::find(m_beginPosition, m_endPosition, "--" + command) != m_endPosition;
    }

    Aws::Crt::String CommandLineUtils::GetCommand(Aws::Crt::String command)
    {
        const char **itr = std::find(m_beginPosition, m_endPosition, "--" + command);
        if (itr != m_endPosition && ++itr != m_endPosition)
        {
            return Aws::Crt::String(*itr);
        }
        return "";
    }

    Aws::Crt::String CommandLineUtils::GetCommandOrDefault(Aws::Crt::String command, Aws::Crt::String commandDefault)
    {
        if (HasCommand(command))
        {
            return Aws::Crt::String(GetCommand(command));
        }
        return commandDefault;
    }

    Aws::Crt::String CommandLineUtils::GetCommandRequired(Aws::Crt::String command)
    {
        if (HasCommand(command))
        {
            return GetCommand(command);
        }
        PrintHelp();
        fprintf(stderr, "Missing required argument: --%s\n", command.c_str());
        exit(-1);
    }

    Aws::Crt::String CommandLineUtils::GetCommandRequired(Aws::Crt::String command, Aws::Crt::String commandAlt)
    {
        if (HasCommand(commandAlt))
        {
            return GetCommand(commandAlt);
        }
        return GetCommandRequired(command);
    }

    void CommandLineUtils::PrintHelp()
    {
        fprintf(stdout, "Usage:\n");
        fprintf(stdout, "%s", m_programName.c_str());

        for (auto const &pair : m_registeredCommands)
        {
            fprintf(stdout, " --%s %s", pair.first.c_str(), pair.second.m_exampleInput.c_str());
        }

        fprintf(stdout, "\n\n");

        for (auto const &pair : m_registeredCommands)
        {
            fprintf(stdout, "* %s:\t\t%s\n", pair.first.c_str(), pair.second.m_helpOutput.c_str());
        }

        fprintf(stdout, "\n");
    }

    void CommandLineUtils::AddCommonMQTTCommands()
    {
        RegisterCommand(m_cmd_endpoint, "<str>", "The endpoint of the mqtt server not including a port.");
        RegisterCommand(
            m_cmd_ca_file, "<path>", "Path to AmazonRootCA1.pem (optional, system trust store used by default).");
        RegisterCommand(m_cmd_is_ci, "<str>", "If present the sample will run in CI mode.");
    }

    void CommandLineUtils::AddCommonKeyCertCommands()
    {
        RegisterCommand(m_cmd_key_file, "<path>", "Path to your key in PEM format.");
        RegisterCommand(m_cmd_cert_file, "<path>", "Path to your client certificate in PEM format.");
    }

    void CommandLineUtils::AddCommonKeyMediaCommands()
    {
        // RegisterCommand(m_cmd_channel_name, "<str>", "KVS Signaling channel name(optional, default='TestChannel'");
        RegisterCommand(m_cmd_media_type, "<str>", "Media type(optional, video-only/audio-video default='video-only'");
        RegisterCommand(m_cmd_media_source_type, "<str>", "Media source type(optional, testsrc/devicesrc/rtspsrc default='devicesrc'");
        RegisterCommand(m_cmd_rtsp_uri, "<str>", "RTSP URI, Mandatory if the media source type is rtspsrc");
    }

    void CommandLineUtils::AddCommonTopicMessageCommands()
    {
        RegisterCommand(
            m_cmd_message, "<str>", "The message to send in the payload (optional, default='Hello world!')");
        RegisterCommand(m_cmd_topic, "<str>", "Topic to publish, subscribe to. (optional, default='test/topic')");
    }

    void CommandLineUtils::AddLoggingCommands()
    {
        RegisterCommand(
            m_cmd_verbosity,
            "<log level>",
            "The logging level to use. Choices are 'Trace', 'Debug', 'Info', 'Warn', 'Error', 'Fatal', and 'None'. "
            "(optional, default='none')");
        RegisterCommand(
            m_cmd_log_file,
            "<str>",
            "File to write logs to. If not provided, logs will be written to stdout. "
            "(optional, default='none')");
    }

    void CommandLineUtils::StartLoggingBasedOnCommand(Aws::Crt::ApiHandle *apiHandle)
    {
        // Process logging command
        if (HasCommand("verbosity"))
        {
            Aws::Crt::LogLevel logLevel = Aws::Crt::LogLevel::None;
            Aws::Crt::String verbosity = GetCommand(m_cmd_verbosity);
            if (verbosity == "Fatal")
            {
                logLevel = Aws::Crt::LogLevel::Fatal;
            }
            else if (verbosity == "Error")
            {
                logLevel = Aws::Crt::LogLevel::Error;
            }
            else if (verbosity == "Warn")
            {
                logLevel = Aws::Crt::LogLevel::Warn;
            }
            else if (verbosity == "Info")
            {
                logLevel = Aws::Crt::LogLevel::Info;
            }
            else if (verbosity == "Debug")
            {
                logLevel = Aws::Crt::LogLevel::Debug;
            }
            else if (verbosity == "Trace")
            {
                logLevel = Aws::Crt::LogLevel::Trace;
            }
            else
            {
                logLevel = Aws::Crt::LogLevel::None;
            }

            if (HasCommand("log_file"))
            {
                apiHandle->InitializeLogging(logLevel, GetCommand(m_cmd_log_file).c_str());
            }
            else
            {
                apiHandle->InitializeLogging(logLevel, stderr);
            }
        }
    }

    static void s_addLoggingSendArgumentsStartLogging(
        int argc,
        char *argv[],
        Aws::Crt::ApiHandle *api_handle,
        CommandLineUtils *cmdUtils)
    {
        cmdUtils->AddLoggingCommands();
        const char **const_argv = (const char **)argv;
        cmdUtils->SendArguments(const_argv, const_argv + argc);
        cmdUtils->StartLoggingBasedOnCommand(api_handle);
        if (cmdUtils->HasCommand(m_cmd_help))
        {
            cmdUtils->PrintHelp();
            exit(-1);
        }
    }

    static void s_parseCommonMQTTCommands(CommandLineUtils *cmdUtils, cmdData *cmdData)
    {
        cmdData->input_endpoint = cmdUtils->GetCommandRequired(m_cmd_endpoint);
        if (cmdUtils->HasCommand(m_cmd_ca_file))
        {
            cmdData->input_ca = cmdUtils->GetCommand(m_cmd_ca_file);
        }
        cmdData->input_isCI = cmdUtils->HasCommand(m_cmd_is_ci);
    }

    cmdData parseSampleInputShadow(int argc, char *argv[], Aws::Crt::ApiHandle *api_handle)
    {
        Utils::CommandLineUtils cmdUtils = Utils::CommandLineUtils();
        cmdUtils.RegisterProgramName("shadow_sync");
        cmdUtils.AddCommonMQTTCommands();
        // m_cmd_cert_file, m_cmd_key_file
        cmdUtils.AddCommonKeyCertCommands();
        cmdUtils.RegisterCommand(m_cmd_thing_name, "<str>", "The name of your IOT thing");
        cmdUtils.RegisterCommand(
            m_cmd_shadow_property,
            "<str>",
            "The name of the shadow property you want to change (optional, default='pan')");
        cmdUtils.RegisterCommand(m_cmd_credential_endpoint, "<str>", "IoT Credential endpoint (optional, default='')");
        cmdUtils.RegisterCommand(m_cmd_role_alias, "<str>", "The role alias name of your IOT thing(optional, default=''");
        // m_cmd_channel_name, m_cmd_media_type, m_cmd_media_source_type, m_cmd_rtsp_uri
        cmdUtils.AddCommonKeyMediaCommands();
        cmdUtils.RegisterCommand(m_cmd_client_id, "<str>", "Client id to use (optional, default='test-*')");

        s_addLoggingSendArgumentsStartLogging(argc, argv, api_handle, &cmdUtils);

        cmdData returnData = cmdData();
        s_parseCommonMQTTCommands(&cmdUtils, &returnData);
        returnData.input_cert = cmdUtils.GetCommandRequired(m_cmd_cert_file);
        returnData.input_key = cmdUtils.GetCommandRequired(m_cmd_key_file);
        returnData.input_thingName = cmdUtils.GetCommandRequired(m_cmd_thing_name);
        returnData.input_shadowProperty = cmdUtils.GetCommandOrDefault(m_cmd_shadow_property, "pan");
        returnData.input_credentialEndpoint = cmdUtils.GetCommandOrDefault(m_cmd_credential_endpoint, "");
        returnData.input_roleAlias = cmdUtils.GetCommandOrDefault(m_cmd_role_alias, "");
        // returnData.input_channelName = cmdUtils.GetCommandOrDefault(m_cmd_channel_name, "TestChannel")
        returnData.input_kvsRegion = cmdUtils.GetCommandOrDefault(m_cmd_kvs_region, "");
        returnData.input_mediaType = cmdUtils.GetCommandOrDefault(m_cmd_media_type, "video-only");
        returnData.input_mediaSourceType = cmdUtils.GetCommandOrDefault(m_cmd_media_source_type, "devicesrc");
        returnData.input_rtspUri = cmdUtils.GetCommandOrDefault(m_cmd_rtsp_uri, "");
        returnData.input_clientId =
            cmdUtils.GetCommandOrDefault(m_cmd_client_id, Aws::Crt::String("test-") + Aws::Crt::UUID().ToString());
        return returnData;
    }

} // namespace Utils