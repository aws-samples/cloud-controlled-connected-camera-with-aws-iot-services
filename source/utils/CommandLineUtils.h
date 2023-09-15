/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */
#pragma once

#include <aws/crt/Api.h>
#include <aws/crt/Types.h>
#include <aws/iot/MqttClient.h>

namespace Utils
{
    /**
     * A struct to hold the command line options that can be passed to the program from the terminal/console.
     */
    struct CommandLineOption
    {
        Aws::Crt::String m_commandName;
        Aws::Crt::String m_exampleInput;
        Aws::Crt::String m_helpOutput;

        CommandLineOption(
            Aws::Crt::String inputName,
            Aws::Crt::String inputExampleInput,
            Aws::Crt::String inputHelp = "")
        {
            m_commandName = std::move(inputName);
            m_exampleInput = std::move(inputExampleInput);
            m_helpOutput = std::move(inputHelp);
        }
    };

    /**
     * A helper class that makes it easier to register, find, and parse commands passed to the program from the
     * terminal/console.
     */
    class CommandLineUtils
    {
    public:
        CommandLineUtils();

        /**
         * Changes the program name to the name given. The program name is shown when calling help and showing all the
         * commands.
         * @param newProgramName The program name to show when executing PrintHelp
         */
        void RegisterProgramName(Aws::Crt::String newProgramName);

        /**
         * Adds a new command to the utility. Used to show command data when printing all commands.
         * @param newCommand The command struct containing the new command/argument data
         */
        void RegisterCommand(CommandLineOption newCommand);
        /**
         * Adds a new command to the utility. Used to show command data when printing all commands.
         * @param commandName The name of the command
         * @param exampleInput Example input for the command (example "<endpoint>")
         * @param helpOutput The message to show with the command when printing all commands via help
         */
        void RegisterCommand(
            Aws::Crt::String commandName,
            Aws::Crt::String exampleInput,
            Aws::Crt::String helpOutput = "");

        /**
         * Removes the command if it has already been registered
         * @param commandName
         */
        void RemoveCommand(Aws::Crt::String commandName);

        /**
         * Updates the help text of a registered command. If the given command does not exist, nothing happens
         * @param commandName The name of the command
         * @param newCommandHelp
         */
        void UpdateCommandHelp(Aws::Crt::String commandName, Aws::Crt::String newCommandHelp);

        /**
         * Called to give the class a copy of the begin and end character pointers that contain the arguments from the
         * terminal/console
         * @param argv The beginning of terminal/console input
         * @param argc The end of terminal/console input
         */
        void SendArguments(const char **argv, const char **argc);

        /**
         * Returns true if the command was inputted into the terminal/console
         *
         * You must call SendArguments and pass terminal/console input first in order for the function to work.
         *
         * @param CommandName The name of the command you are looking for
         * @return true If the command is found
         * @return false If the command is not found
         */
        bool HasCommand(Aws::Crt::String CommandName);

        /**
         * Gets the value of the command passed into the console/terminal. This function assumes the command exists
         * and was passed into the program through the console/terminal.
         *
         * You must call SendArguments and pass console/terminal input first in order for the function to work.
         *
         * @param CommandName The name of the command you want to get the value of
         * @return Aws::Crt::String The value passed into the program at the command name
         */
        Aws::Crt::String GetCommand(Aws::Crt::String CommandName);

        /**
         * Gets the value of the command passed into the console/terminal if it exists, otherwise it returns
         * whatever value is appsed into CommandDefault
         *
         * You must call SendArguments and pass console/terminal input first in order for the function to work.
         *
         * @param CommandName The name of the command you want to get the value of
         * @param CommandDefault The value to assign if the command does not exist
         * @return Aws::Crt::String The value passed into the program at the command name
         */
        Aws::Crt::String GetCommandOrDefault(Aws::Crt::String CommandName, Aws::Crt::String CommandDefault);

        /**
         * Gets the value of the command passed into the console/terminal if it exists. If it does not exist,
         * the program will exit with an error message.
         *
         * @param CommandName The name of the command you want to get the value of
         * @return Aws::Crt::String The value passed into the program at the command name
         */
        Aws::Crt::String GetCommandRequired(Aws::Crt::String CommandName);

        /**
         * Gets the value of the command passed into the console/terminal if it exists. If it does not exist,
         * the program will exit with an error message.
         *
         * @param CommandName The name of the command you want to get the value of
         * @param CommandNameAlt The alternate name of the command you want to get the value of
         * @return Aws::Crt::String The value passed into the program at the command name
         */
        Aws::Crt::String GetCommandRequired(Aws::Crt::String CommandName, Aws::Crt::String CommandNameAlt);

        /**
         * Prints to the console/terminal all of the commands and their descriptions.
         */
        void PrintHelp();

        /**
         * A helper function that adds endpoint and ca_file commands
         */
        void AddCommonMQTTCommands();

        /**
         * A helper function that adds key and cert commands
         */
        void AddCommonKeyCertCommands();

        /**
         * A helper function that adds media commands for KVS
         */
        void AddCommonKeyMediaCommands();

        /**
         * A helper function that adds topic and message commands
         */
        void AddCommonTopicMessageCommands();

        /**
         * A helper function that adds the verbosity command for controlling logging in the samples
         */
        void AddLoggingCommands();

        /**
         * Starts logging based on the result of the verbosity command
         */
        void StartLoggingBasedOnCommand(Aws::Crt::ApiHandle *apiHandle);

    private:
        Aws::Crt::String m_programName = "Application";
        const char **m_beginPosition = nullptr;
        const char **m_endPosition = nullptr;
        Aws::Crt::Map<Aws::Crt::String, CommandLineOption> m_registeredCommands;

        Aws::Iot::MqttClient m_internal_client;
        Aws::Crt::Http::HttpClientConnectionProxyOptions GetProxyOptionsForMQTTConnection();
        std::shared_ptr<Aws::Crt::Mqtt::MqttConnection> GetClientConnectionForMQTTConnection(
            Aws::Iot::MqttClient *client,
            Aws::Iot::MqttClientConnectionConfigBuilder *clientConfigBuilder);
    };

    struct cmdData
    {
        // General use
        Aws::Crt::String input_endpoint;
        Aws::Crt::String input_cert;
        Aws::Crt::String input_key;
        Aws::Crt::String input_ca;
        Aws::Crt::String input_clientId;
        uint64_t input_port;
        bool input_isCI;
        // Proxy
        Aws::Crt::String input_proxyHost;
        uint64_t input_proxyPort;
        // PubSub
        Aws::Crt::String input_topic;
        Aws::Crt::String input_message;
        uint64_t input_count;
        // Services (Shadow, Jobs, Greengrass, etc)
        Aws::Crt::String input_thingName;
        Aws::Crt::String input_mode;
        // Shadow
        Aws::Crt::String input_shadowProperty;
        // KVS
        Aws::Crt::String input_kvsRegion;
        Aws::Crt::String input_credentialEndpoint;
        Aws::Crt::String input_roleAlias;
        // WebRTC
        // Aws::Crt::String input_channelName;
        Aws::Crt::String input_mediaType;
        Aws::Crt::String input_mediaSourceType;
        Aws::Crt::String input_rtspUri;
    };

    cmdData parseSampleInputShadow(int argc, char *argv[], Aws::Crt::ApiHandle *api_handle);

} // namespace Utils