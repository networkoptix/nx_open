/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include <vector>
#include <string>

#include "Value.h"

namespace ArgusSamples
{

/**
 * Handles command line options.
 */
class Options
{
public:
    explicit Options(const char *programName);
    ~Options();

    bool initialize();

    /**
     * Defines a single option
     */
    class Option
    {
    public:
        /**
         * Type
         */
        typedef enum
        {
            TYPE_ACTION,    ///< triggers an action
            TYPE_OPTION     ///< sets an option
        } Type;

        /**
         * Argument flags
         */
        typedef enum
        {
            FLAG_NO_ARGUMENT,       ///< requires no argument
            FLAG_OPTIONAL_ARGUMENT, ///< optionally takes an argument
            FLAG_REQUIRED_ARGUMENT  ///< requires an argument
        } Flag;

        /**
         * Call back function.
         *
         * @param [in] usrPtr   user pointer
         * @param [in] optArg   optional argument string, NULL when there is no argument
         */
        typedef bool (*CallBackFunc)(void *usrPtr, const char *optArg);

        /**
         * Construct an option
         *
         * @param name [in] long option name
         * @param shortName [in] short option name
         * @param argument [in] argument
         * @param type [in] option type
         * @param flags [in] flags
         * @param usage [in] a string describing the usage
         * @param function [in] callback function
         * @param userPtr [in] user pointer
         */
        explicit Option(std::string name, char shortName, std::string argument, Type type,
                Flag flags, std::string usage, CallBackFunc function, void *userPtr = NULL)
            : m_name(name)
            , m_shortName(shortName)
            , m_argument(argument)
            , m_type(type)
            , m_flags(flags)
            , m_usage(usage)
            , m_function(function)
            , m_userPtr(userPtr)
        {
        }
        /**
         * Construct an option from a Value. The final usage string is build from the given usage
         * and the default and valid values of the given value variable.
         *
         * @param name [in] long option name
         * @param shortName [in] short option name
         * @param argument [in] argument
         * @param value [in] value to construct the option from
         * @param usage [in] a string describing the usage
         * @param function [in] callback function
         * @param userPtr [in] user pointer
         */
        template<typename T> explicit Option(std::string name, char shortName,
            std::string argument, Value<T> &value, std::string usage, CallBackFunc function,
            void *userPtr = NULL)
            : m_name(name)
            , m_shortName(shortName)
            , m_argument(argument)
            , m_type(TYPE_OPTION)
            , m_flags(argument.empty() ? FLAG_NO_ARGUMENT : FLAG_REQUIRED_ARGUMENT)
            , m_function(function)
            , m_userPtr(userPtr)
        {
            std::ostringstream msg;
            msg << usage << " " << value.getValidator()->getValidValuesMessage() <<
                " Default is '" << value.toString() << "'.";
            m_usage = msg.str();
        }
        ~Option()
        {
        }

        std::string m_name;         //!< option name
        char m_shortName;           //!< option short name
        std::string m_argument;     //!< argument name
        Type m_type;                //!< option type
        Flag m_flags;               //!< option flags
        std::string m_usage;        //!< usage message
        CallBackFunc m_function;    //!< callback function
        void *m_userPtr;            //!< user pointer passed to callback function
    };

    /**
     * Print the usage message.
     */
    bool usage();

    /**
     * Parse the command line options
     *
     * @param [in] argc     argument count
     * @param [in] argv     argument values
     */
    bool parse(const int argc, char * const *argv);

    /**
     * Add a option
     *
     * @param [in] option   option to add
     * @param [in] userPtr  user pointer passed to callback function
     */
    bool addOption(const Option &option, void *userPtr = NULL);

    /**
     * Add multiple options
     *
     * @param [in] count    how many options to add
     * @param [in] options  option array
     * @param [in] userPtr  user pointer passed to callback functions
     */
    bool addOptions(size_t count, const Option *options, void *userPtr = NULL);

    /**
     * Add test to the description, will be printed with the usage message
     *
     * @param [in] description  Description
     */
    bool addDescription(const char *description);

    /**
     * Request exit after the current option, called from callback function.
     */
    bool requestExit();

    /**
     * Has the exit been requested?
     */
    bool requestedExit() const;

    /**
     * help option callback
     */
    static bool printHelp(void *userPtr, const char *optArg);

    /**
     * exit option callback
     */
    static bool exit(void *userPtr, const char *optArg);

private:
    bool m_initialized;
    bool m_requestExit;
    std::string m_programName;
    std::string m_description;
    std::vector<Option> m_options;

    /**
     * Hide default constructor
     */
    Options();
};

}; // namespace ArgusSamples

#endif // OPTIONS_H
