// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

static bool has_source(const std::vector<CLI::ValueSource> &records, CLI::SourceKind kind) {
    return std::any_of(records.begin(), records.end(), [kind](const CLI::ValueSource &src) {
        return src.kind == kind;
    });
}

TEST_CASE("SourceTracking: DisabledByDefault", "[sourcetracking]") {
    TApp app;
    int val{0};
    auto *opt = app.app.add_option("--val", val);
    app.args = {"--val", "5"};
    app.run();
    CHECK(5 == val);
    CHECK(opt->sources().empty());
    CHECK(opt->reduced_sources().empty());
    CHECK(opt->overridden_sources().empty());
}

TEST_CASE("SourceTracking: CommandLineTokens", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    int val{0};
    auto *opt = app.app.add_option("--val", val);
    app.args = {"--val", "5"};
    app.run();
    REQUIRE(opt->sources().size() == 1);
    const auto &src = opt->sources()[0];
    CHECK(src.kind == CLI::SourceKind::CommandLine);
    CHECK(src.token_begin == 1);
    CHECK(src.token_end == 2);
    CHECK(src.value == "5");
    CHECK_FALSE(src.value_hidden);
    CHECK(src.describe().find("[1, 2)") != std::string::npos);
    // attached to the option token itself for --opt=value forms
    TApp app2;
    app2.app.track_sources();
    int val2{0};
    auto *opt2 = app2.app.add_option("--val", val2);
    app2.args = {"--val=7"};
    app2.run();
    REQUIRE(opt2->sources().size() == 1);
    CHECK(opt2->sources()[0].token_begin == 0);
    CHECK(opt2->sources()[0].token_end == 1);
    CHECK(opt2->sources()[0].value == "7");
}

TEST_CASE("SourceTracking: EnvironmentSource", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    int val{0};
    auto *opt = app.app.add_option("--val", val)->envname("CLI11_SOURCE_TEST_VAL");
    put_env("CLI11_SOURCE_TEST_VAL", "33");
    app.run();
    unset_env("CLI11_SOURCE_TEST_VAL");
    CHECK(33 == val);
    REQUIRE(opt->sources().size() == 1);
    const auto &src = opt->sources()[0];
    CHECK(src.kind == CLI::SourceKind::Environment);
    CHECK(src.env_name == "CLI11_SOURCE_TEST_VAL");
    CHECK(src.value == "33");
    CHECK(src.describe().find("CLI11_SOURCE_TEST_VAL") != std::string::npos);
}

TEST_CASE("SourceTracking: ConfigSource", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "# a comment" << '\n';
        out << "val=11" << '\n';
        out << "[mysection]" << '\n';
        out << "val=22" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    int val{0};
    auto *opt = app.app.add_option("--val", val);
    int subval{0};
    auto *sub = app.app.add_subcommand("mysection");
    auto *subopt = sub->add_option("--val", subval);
    app.args = {"--config", tmpini};
    app.run();
    CHECK(11 == val);
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->sources()[0].line == 2);
    CHECK(opt->sources()[0].section.empty());
    CHECK(opt->sources()[0].file == std::string(tmpini));
    CHECK(opt->sources()[0].value == "11");
    CHECK(22 == subval);
    REQUIRE(subopt->sources().size() == 1);
    CHECK(subopt->sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(subopt->sources()[0].line == 4);
    CHECK(subopt->sources()[0].section == "mysection");
    CHECK(subopt->sources()[0].file == std::string(tmpini));
}

TEST_CASE("SourceTracking: ScalarOverrideChain", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "other=1" << '\n';
        out << "val=from_config" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    std::string val;
    auto *opt = app.app.add_option("--val", val)->envname("CLI11_SOURCE_TEST_VAL");
    put_env("CLI11_SOURCE_TEST_VAL", "from_env");
    app.args = {"--config", tmpini, "--val", "from_cmdline"};
    app.run();
    unset_env("CLI11_SOURCE_TEST_VAL");
    // command line wins over environment and config
    CHECK(val == "from_cmdline");
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[0].token_begin == 3);
    CHECK(opt->sources()[0].token_end == 4);
    CHECK(opt->sources()[0].value == "from_cmdline");
    // both lower priority inputs are recorded as overridden
    REQUIRE(opt->overridden_sources().size() == 2);
    CHECK(opt->overridden_sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->overridden_sources()[0].value == "from_config");
    CHECK(opt->overridden_sources()[0].line == 2);
    CHECK(opt->overridden_sources()[0].file == std::string(tmpini));
    CHECK(opt->overridden_sources()[1].kind == CLI::SourceKind::Environment);
    CHECK(opt->overridden_sources()[1].value == "from_env");
    CHECK(opt->overridden_sources()[1].env_name == "CLI11_SOURCE_TEST_VAL");
}

TEST_CASE("SourceTracking: ConfigOverridesEnv", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "val=from_config" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    std::string val;
    auto *opt = app.app.add_option("--val", val)->envname("CLI11_SOURCE_TEST_VAL");
    put_env("CLI11_SOURCE_TEST_VAL", "from_env");
    app.args = {"--config", tmpini};
    app.run();
    unset_env("CLI11_SOURCE_TEST_VAL");
    // config files are processed before environment variables, so the config value wins
    CHECK(val == "from_config");
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->sources()[0].value == "from_config");
    REQUIRE(opt->overridden_sources().size() == 1);
    CHECK(opt->overridden_sources()[0].kind == CLI::SourceKind::Environment);
    CHECK(opt->overridden_sources()[0].value == "from_env");
    CHECK(opt->overridden_sources()[0].env_name == "CLI11_SOURCE_TEST_VAL");
}

TEST_CASE("SourceTracking: MultiValueOverrideChain", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "vec=[cfg1, cfg2]" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    std::vector<std::string> vec;
    auto *opt = app.app.add_option("--vec", vec)->envname("CLI11_SOURCE_TEST_VEC")->expected(1)->take_all();
    put_env("CLI11_SOURCE_TEST_VEC", "from_env");
    app.args = {"--config", tmpini, "--vec", "c1", "--vec", "c2"};
    app.run();
    unset_env("CLI11_SOURCE_TEST_VEC");
    CHECK(vec == std::vector<std::string>({"c1", "c2"}));
    // each retained value is tracked individually
    REQUIRE(opt->sources().size() == 2);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[0].value == "c1");
    CHECK(opt->sources()[0].token_begin == 3);
    CHECK(opt->sources()[0].token_end == 4);
    CHECK(opt->sources()[1].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[1].value == "c2");
    CHECK(opt->sources()[1].token_begin == 5);
    CHECK(opt->sources()[1].token_end == 6);
    // overridden inputs: two config values then the environment value
    REQUIRE(opt->overridden_sources().size() == 3);
    CHECK(opt->overridden_sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->overridden_sources()[0].value == "cfg1");
    CHECK(opt->overridden_sources()[0].line == 1);
    CHECK(opt->overridden_sources()[1].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->overridden_sources()[1].value == "cfg2");
    CHECK(opt->overridden_sources()[2].kind == CLI::SourceKind::Environment);
    CHECK(opt->overridden_sources()[2].value == "from_env");
}

TEST_CASE("SourceTracking: TakeLastVsJoin", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    std::string val;
    auto *opt = app.app.add_option("--val", val)->expected(1)->take_last();
    app.args = {"--val", "one", "--val", "two", "--val", "three"};
    app.run();
    // TakeLast keeps only the last value
    CHECK(val == "three");
    REQUIRE(opt->sources().size() == 3);
    CHECK(opt->sources()[0].value == "one");
    CHECK(opt->sources()[1].value == "two");
    CHECK(opt->sources()[2].value == "three");
    auto reduced = opt->reduced_sources();
    REQUIRE(reduced.size() == 1);
    CHECK(reduced[0].kind == CLI::SourceKind::CommandLine);
    CHECK(reduced[0].value == "three");
    CHECK(reduced[0].token_begin == 5);
    CHECK(reduced[0].token_end == 6);
    CHECK(reduced[0].contributors == 1);

    // switch the policy to Join and reparse: the reduced record now merges all inputs
    opt->join(',');
    app.run();
    CHECK(val == "one,two,three");
    REQUIRE(opt->sources().size() == 3);
    reduced = opt->reduced_sources();
    REQUIRE(reduced.size() == 1);
    CHECK(reduced[0].kind == CLI::SourceKind::CommandLine);
    CHECK(reduced[0].value == "one,two,three");
    CHECK(reduced[0].contributors == 3);
    CHECK(reduced[0].token_begin == 1);
    CHECK(reduced[0].token_end == 6);
    CHECK(reduced[0].describe().find("merged from 3") != std::string::npos);
}

TEST_CASE("SourceTracking: TakeFirst", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    std::string val;
    auto *opt = app.app.add_option("--val", val)->expected(1)->take_first();
    app.args = {"--val", "one", "--val", "two"};
    app.run();
    CHECK(val == "one");
    auto reduced = opt->reduced_sources();
    REQUIRE(reduced.size() == 1);
    CHECK(reduced[0].value == "one");
    CHECK(reduced[0].token_begin == 1);
    CHECK(reduced[0].token_end == 2);
}

TEST_CASE("SourceTracking: Subcommand", "[sourcetracking]") {
    TApp app;
    std::string val;
    auto *sub = app.app.add_subcommand("sub");
    auto *opt = sub->add_option("--val", val);
    // enable after construction: applies recursively to subcommands and their options
    app.app.track_sources();
    app.args = {"sub", "--val", "x"};
    app.run();
    CHECK(val == "x");
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[0].token_begin == 2);
    CHECK(opt->sources()[0].token_end == 3);
    CHECK(opt->sources()[0].value == "x");
}

TEST_CASE("SourceTracking: Aliases", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "long=from_config" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    std::string val;
    auto *opt = app.app.add_option("-s,--long", val);
    // use the short alias on the command line; the config file uses the long name
    app.args = {"--config", tmpini, "-s", "from_cmdline"};
    app.run();
    CHECK(val == "from_cmdline");
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[0].token_begin == 3);
    CHECK(opt->sources()[0].token_end == 4);
    REQUIRE(opt->overridden_sources().size() == 1);
    CHECK(opt->overridden_sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->overridden_sources()[0].value == "from_config");
    CHECK(opt->overridden_sources()[0].line == 1);
}

TEST_CASE("SourceTracking: OptionDefaultsPropagation", "[sourcetracking]") {
    TApp app;
    // enabling through option defaults applies to options added afterwards
    app.app.option_defaults()->track_source();
    int val{0};
    auto *opt = app.app.add_option("--val", val);
    CHECK(opt->get_track_source());
    app.args = {"--val", "3"};
    app.run();
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
}

TEST_CASE("SourceTracking: ConfigIncludeBoundary", "[sourcetracking]") {
    // include directives are not resolved by the config parser; an unknown `include` key is
    // treated like any other unknown config entry (the boundary of include support)
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "include=other.ini" << '\n';
        out << "val=5" << '\n';
    }
    {
        // by default unknown config entries (such as an include directive) are ignored
        TApp app;
        app.app.track_sources();
        app.app.set_config("--config");
        int val{0};
        auto *opt = app.app.add_option("--val", val);
        app.args = {"--config", tmpini};
        app.run();
        CHECK(5 == val);
        REQUIRE(opt->sources().size() == 1);
        CHECK(opt->sources()[0].kind == CLI::SourceKind::ConfigFile);
        CHECK(opt->sources()[0].line == 2);
        CHECK_FALSE(has_source(opt->sources(), CLI::SourceKind::Unknown));
    }
    {
        // in strict mode the unresolved include directive is reported as a config extra
        TApp app;
        app.app.track_sources();
        app.app.allow_config_extras(CLI::ConfigExtrasMode::Error);
        app.app.set_config("--config");
        int val{0};
        app.app.add_option("--val", val);
        app.args = {"--config", tmpini};
        CHECK_THROWS_AS(app.run(), CLI::ConfigError);
    }
}

TEST_CASE("SourceTracking: SensitiveValuesHidden", "[sourcetracking]") {
    TempFile tmpini{"TestSourceTracking.ini"};
    {
        std::ofstream out{tmpini};
        out << "secret=config_secret" << '\n';
    }
    TApp app;
    app.app.track_sources();
    app.app.set_config("--config");
    std::string secret;
    auto *opt = app.app.add_option("--secret", secret)->sensitive();
    app.args = {"--config", tmpini, "--secret", "cmdline_secret"};
    app.run();
    CHECK(secret == "cmdline_secret");
    // only locations are kept, never values
    REQUIRE(opt->sources().size() == 1);
    CHECK(opt->sources()[0].kind == CLI::SourceKind::CommandLine);
    CHECK(opt->sources()[0].token_begin == 3);
    CHECK(opt->sources()[0].token_end == 4);
    CHECK(opt->sources()[0].value.empty());
    CHECK(opt->sources()[0].value_hidden);
    CHECK(opt->sources()[0].describe().find("cmdline_secret") == std::string::npos);
    CHECK(opt->sources()[0].describe().find("<value hidden>") != std::string::npos);
    REQUIRE(opt->overridden_sources().size() == 1);
    CHECK(opt->overridden_sources()[0].kind == CLI::SourceKind::ConfigFile);
    CHECK(opt->overridden_sources()[0].value.empty());
    CHECK(opt->overridden_sources()[0].value_hidden);
    CHECK(opt->overridden_sources()[0].describe().find("config_secret") == std::string::npos);
}

TEST_CASE("SourceTracking: SensitiveValidationFailure", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    int secret{0};
    app.app.add_option("--secret", secret)->sensitive()->check(CLI::Range(1, 10));
    app.args = {"--secret", "42"};
    try {
        app.run();
        FAIL("expected a ValidationError");
    } catch(const CLI::ValidationError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("42") == std::string::npos);
        CHECK(msg.find("<redacted>") != std::string::npos);
    }
}

TEST_CASE("SourceTracking: SensitiveConversionFailure", "[sourcetracking]") {
    TApp app;
    app.app.track_sources();
    int secret{0};
    app.app.add_option("--secret", secret)->sensitive();
    app.args = {"--secret", "not_a_number"};
    try {
        app.run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("not_a_number") == std::string::npos);
        CHECK(msg.find("<redacted>") != std::string::npos);
    }
    // non-sensitive options still report the failing value
    TApp app2;
    int plain{0};
    app2.app.add_option("--plain", plain);
    app2.args = {"--plain", "not_a_number"};
    try {
        app2.run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        CHECK(std::string(err.what()).find("not_a_number") != std::string::npos);
    }
}
