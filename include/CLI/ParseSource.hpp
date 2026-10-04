// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
// [CLI11:public_includes:end]

#include "Macros.hpp"

namespace CLI {
// [CLI11:parse_source_hpp:verbatim]

/// The kind of source that produced a parsed value
enum class SourceType : std::uint8_t {
    CommandLine = 0,  //!< value came from one or more command-line tokens
    Environment = 1,  //!< value came from an environment variable
    ConfigFile = 2,   //!< value came from a configuration file
    Default = 3,      //!< value is an internally generated default
};

/// A description of where a single parsed value (or a merged group of values) originated.
///
/// For command-line values the inclusive @ref token_start and @ref token_end indices refer
/// to the original @c argv array (with the program name at index 0). For string-based parsing
/// the indices refer to the tokens as they were split (0-based). Environment values carry the
/// variable name and configuration values carry the file path, section and 1-based line number.
struct ParseSource {
    /// The kind of source
    SourceType type{SourceType::CommandLine};

    /// For CommandLine: inclusive index of the first token that supplied the value
    std::size_t token_start{0};
    /// For CommandLine: inclusive index of the last token that supplied the value
    std::size_t token_end{0};

    /// For Environment: the name of the environment variable
    std::string env_name{};

    /// For ConfigFile: the path of the configuration file
    std::string config_file{};
    /// For ConfigFile: the section the value was found in (dot-separated parents)
    std::string config_section{};
    /// For ConfigFile: the 1-based line number the value was found on
    int config_line{0};

    /// True if the value came from the command line
    CLI11_NODISCARD bool is_command_line() const noexcept { return type == SourceType::CommandLine; }
    /// True if the value came from an environment variable
    CLI11_NODISCARD bool is_environment() const noexcept { return type == SourceType::Environment; }
    /// True if the value came from a configuration file
    CLI11_NODISCARD bool is_config_file() const noexcept { return type == SourceType::ConfigFile; }
    /// True if the value is an internally generated default
    CLI11_NODISCARD bool is_default() const noexcept { return type == SourceType::Default; }
};

/// A retained value together with the source(s) that produced it.
///
/// Normally a retained value has a single source. When a MultiOptionPolicy merges several
/// inputs into one (Join or Sum) the single retained value carries all the contributing sources.
/// For sensitive options the @ref value is redacted; the sources are still recorded.
struct SourcedValue {
    /// The retained value (redacted for sensitive options)
    std::string value{};
    /// The source(s) that produced this value
    std::vector<ParseSource> sources{};
};

// [CLI11:parse_source_hpp:end]
}  // namespace CLI
