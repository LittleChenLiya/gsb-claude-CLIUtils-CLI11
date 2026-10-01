// Copyright (c) 2017-2026, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <string>

TEST_CASE_METHOD(TApp, "SensitiveFlagDefault", "[sensitive]") {
    std::string value;
    auto *opt = app.add_option("--secret", value);
    CHECK(!opt->get_sensitive());
    opt->sensitive();
    CHECK(opt->get_sensitive());
    opt->sensitive(false);
    CHECK(!opt->get_sensitive());
}

TEST_CASE_METHOD(TApp, "SensitiveOptionDefaultsPropagation", "[sensitive]") {
    std::string value;
    app.option_defaults()->sensitive();
    auto *opt = app.add_option("--secret", value);
    CHECK(opt->get_sensitive());
}

TEST_CASE_METHOD(TApp, "SensitiveConversionErrorMasked", "[sensitive]") {
    int secret{0};
    int plain{0};
    app.add_option("--secret", secret)->sensitive();
    app.add_option("--plain", plain);

    // both options are given the exact same value; only the sensitive occurrence may be masked
    args = {"--secret", "hunter2", "--plain", "hunter2"};
    try {
        run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("--secret") != std::string::npos);
        CHECK(msg.find("hunter2") == std::string::npos);
        CHECK(msg.find(CLI::detail::sensitive_mask()) != std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveSameValueNormalOptionNotMasked", "[sensitive]") {
    std::string secret;
    int plain{0};
    app.add_option("--secret", secret)->sensitive();
    app.add_option("--plain", plain);

    // the sensitive option holds the same text as the failing normal option;
    // the normal option's error message must keep its own value visible
    args = {"--secret", "hunter2", "--plain", "hunter2"};
    try {
        run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("--plain") != std::string::npos);
        CHECK(msg.find("hunter2") != std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveShortOptionMasked", "[sensitive]") {
    int secret{0};
    app.add_option("-s,--secret", secret)->sensitive();

    args = {"-s", "hunter2"};
    try {
        run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("hunter2") == std::string::npos);
        CHECK(msg.find(CLI::detail::sensitive_mask()) != std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveEqualsFormMasked", "[sensitive]") {
    int secret{0};
    app.add_option("--secret", secret)->sensitive();

    args = {"--secret=hunter2"};
    try {
        run();
        FAIL("expected a ConversionError");
    } catch(const CLI::ConversionError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("hunter2") == std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveMissingValueNoLeak", "[sensitive]") {
    std::string secret;
    std::string other;
    app.add_option("--secret", secret)->sensitive();
    app.add_option("--other", other);

    // --other is missing its value; the error must not leak the sensitive value parsed earlier
    args = {"--secret", "hunter2", "--other"};
    try {
        run();
        FAIL("expected a parse error");
    } catch(const CLI::ParseError &err) {
        CHECK(std::string(err.what()).find("hunter2") == std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveRepeatedOptionNoLeak", "[sensitive]") {
    std::string secret;
    app.add_option("--secret", secret)->sensitive();

    args = {"--secret", "alpha-secret", "--secret", "beta-secret"};
    try {
        run();
        FAIL("expected a parse error");
    } catch(const CLI::ParseError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("alpha-secret") == std::string::npos);
        CHECK(msg.find("beta-secret") == std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "SensitiveHelpDefaultMasked", "[sensitive]") {
    std::string password{"hunter2"};
    std::string user{"bob"};
    app.add_option("--password", password)->capture_default_str()->sensitive();
    app.add_option("--user", user)->capture_default_str();

    const std::string help = app.help();
    CHECK(help.find("hunter2") == std::string::npos);
    CHECK(help.find(CLI::detail::sensitive_mask()) != std::string::npos);
    // a non-sensitive default is still displayed
    CHECK(help.find("[bob]") != std::string::npos);
}

TEST_CASE_METHOD(TApp, "SensitiveValidatorMessageMasked", "[sensitive]") {
    std::string token;
    app.add_option("--token", token)->sensitive()->check([](const std::string &val) {
        return std::string("the token '") + val + "' was rejected";
    });

    args = {"--token", "hunter2"};
    try {
        run();
        FAIL("expected a ValidationError");
    } catch(const CLI::ValidationError &err) {
        const std::string msg = err.what();
        CHECK(msg.find("hunter2") == std::string::npos);
        CHECK(msg.find(CLI::detail::sensitive_mask()) != std::string::npos);
    }
}

TEST_CASE_METHOD(TApp, "NonSensitiveValidatorMessageKept", "[sensitive]") {
    std::string token;
    app.add_option("--token", token)->check([](const std::string &val) {
        return std::string("the token '") + val + "' was rejected";
    });

    args = {"--token", "hunter2"};
    try {
        run();
        FAIL("expected a ValidationError");
    } catch(const CLI::ValidationError &err) {
        CHECK(std::string(err.what()).find("hunter2") != std::string::npos);
    }
}
