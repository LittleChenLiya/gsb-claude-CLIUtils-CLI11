// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <sstream>
#include <string>

TEST_CASE_METHOD(TApp, "SensitiveConversionMasked", "[sensitive]") {
    int secret{};
    app.add_option("--secret", secret)->sensitive();

    args = {"--secret", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ConversionError &e) {
        CHECK_THAT(e.what(), Contains("hunter2"));
        const std::string shown = e.display_message();
        CHECK_THAT(shown, Contains("--secret"));
        CHECK_THAT(shown, Contains("[redacted]"));
        CHECK_THAT(shown, !Contains("hunter2"));
        CHECK(e.has_sensitive_content());
    }
}

TEST_CASE_METHOD(TApp, "SensitiveAndNormalSameValueMaskedDifferently", "[sensitive]") {
    // The same literal is passed to a sensitive and a normal option; only the sensitive
    // occurrence is redacted, no global text replacement is performed.
    int secret{};
    int normal{};
    app.add_option("--secret", secret)->sensitive();
    app.add_option("--normal", normal);

    args = {"--secret", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ConversionError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, Contains("[redacted]"));
        CHECK_THAT(shown, !Contains("hunter2"));
    }

    args = {"--normal", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ConversionError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, Contains("hunter2"));
        CHECK_THAT(shown, !Contains("[redacted]"));
        CHECK(!e.has_sensitive_content());
    }
}

TEST_CASE_METHOD(TApp, "SensitiveShortOptionConversionMasked", "[sensitive]") {
    int secret{};
    app.add_option("-s", secret)->sensitive();

    args = {"-s", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ConversionError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, Contains("[redacted]"));
        CHECK_THAT(shown, !Contains("hunter2"));
    }

    args = {"-shunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ConversionError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, Contains("[redacted]"));
        CHECK_THAT(shown, !Contains("hunter2"));
    }
}

TEST_CASE_METHOD(TApp, "SensitiveMissingValueDoesNotLeak", "[sensitive]") {
    std::string secret;
    app.add_option("-s,--secret", secret)->sensitive();

    args = {"--secret"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ArgumentMismatch &e) {
        CHECK_THAT(e.display_message(), !Contains("hunter2"));
    }
}

TEST_CASE_METHOD(TApp, "SensitiveDuplicateOptionDoesNotLeak", "[sensitive]") {
    std::string secret;
    app.add_option("-s,--secret", secret)->sensitive();

    args = {"-s", "hunter2", "-s", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ArgumentMismatch &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, !Contains("hunter2"));
    }
}

TEST_CASE_METHOD(TApp, "SensitiveValidatorMessageMasked", "[sensitive]") {
    std::string secret;
    app.add_option("--secret", secret)
        ->sensitive()
        ->check([](const std::string &value) { return "value '" + value + "' was rejected"; });

    args = {"--secret", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ValidationError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(e.what(), Contains("hunter2"));
        CHECK_THAT(shown, Contains("[redacted]"));
        CHECK_THAT(shown, !Contains("hunter2"));
        CHECK_THAT(shown, Contains("was rejected"));
    }
}

TEST_CASE_METHOD(TApp, "SensitiveValidatorOnlyMasksSensitiveToken", "[sensitive]") {
    // The input token is embedded once in the validator message; only that range is masked,
    // while the surrounding validator text and option name remain visible.
    std::string secret;
    app.add_option("--secret", secret)
        ->sensitive()
        ->check([](const std::string &value) {
            return "bad token supplied, rejected value is: " + value;
        });

    args = {"--secret", "hunter2"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ValidationError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, !Contains("hunter2"));
        CHECK_THAT(shown, Contains("--secret"));
        CHECK_THAT(shown, Contains("bad token supplied"));
    }
}

TEST_CASE_METHOD(TApp, "SensitiveDefaultMaskedInHelp", "[sensitive]") {
    std::string secret = "hunter2";
    app.add_option("--secret", secret)->sensitive()->capture_default_str();

    const std::string help = app.help();
    CHECK_THAT(help, Contains("[redacted]"));
    CHECK_THAT(help, !Contains("hunter2"));

    // A normal option with the same default still shows its value
    std::string normal = "hunter2";
    app.add_option("--normal", normal)->capture_default_str();
    const std::string help2 = app.help();
    CHECK_THAT(help2, Contains("[redacted]"));
    CHECK_THAT(help2, Contains("hunter2"));
}

TEST_CASE_METHOD(TApp, "SensitiveManualDefaultStrMaskedInHelp", "[sensitive]") {
    app.add_option("--secret")->sensitive()->default_str("hunter2");
    const std::string help = app.help();
    CHECK_THAT(help, Contains("[redacted]"));
    CHECK_THAT(help, !Contains("hunter2"));
}

TEST_CASE_METHOD(TApp, "SensitiveFailureLogMessageMasked", "[sensitive]") {
    int secret{};
    app.add_option("--secret", secret)->sensitive();

    args = {"--secret", "hunter2"};
    std::ostringstream err_stream;
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ParseError &e) {
        app.exit(e, std::cout, err_stream);
    }
    const std::string log = err_stream.str();
    CHECK_THAT(log, Contains("[redacted]"));
    CHECK_THAT(log, !Contains("hunter2"));
}

TEST_CASE_METHOD(TApp, "NormalFailureLogMessageKeepsValue", "[sensitive]") {
    int normal{};
    app.add_option("--normal", normal);

    args = {"--normal", "hunter2"};
    std::ostringstream err_stream;
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ParseError &e) {
        app.exit(e, std::cout, err_stream);
    }
    const std::string log = err_stream.str();
    CHECK_THAT(log, Contains("hunter2"));
    CHECK_THAT(log, !Contains("[redacted]"));
}

TEST_CASE_METHOD(TApp, "SensitiveFlagAccessorRoundTrip", "[sensitive]") {
    auto *opt = app.add_option("--secret")->sensitive(false);
    CHECK(!opt->get_sensitive());
    opt->sensitive();
    CHECK(opt->get_sensitive());
}

TEST_CASE_METHOD(TApp, "SensitiveDelimitedValidatorMessageMasked", "[sensitive]") {
    std::vector<std::string> secrets;
    app.add_option("--secrets", secrets)
        ->sensitive()
        ->delimiter(',')
        ->check([](const std::string &value) {
            return value.empty() ? std::string{} : ("rejected: " + value);
        });

    // First segment fails; the error contains the offending segment only, which is masked.
    args = {"--secrets", "alpha,beta"};
    try {
        run();
        FAIL("Should throw");
    } catch(const CLI::ValidationError &e) {
        const std::string shown = e.display_message();
        CHECK_THAT(shown, !Contains("alpha"));
        CHECK_THAT(shown, Contains("rejected"));
    }
}
