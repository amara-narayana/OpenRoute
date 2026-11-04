#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace openroute::test {

struct TestFailure {
    std::string file;
    int line;
    std::string expression;
    std::string message;
};

class TestCase {
public:
    TestCase(std::string suite, std::string name, std::function<void()> fn)
        : suite_(std::move(suite)), name_(std::move(name)), fn_(std::move(fn)) {}

    void Run() {
        failures_.clear();
        try {
            fn_();
        } catch (const std::exception& ex) {
            AddFailure(__FILE__, __LINE__, "Unhandled exception", ex.what());
        } catch (...) {
            AddFailure(__FILE__, __LINE__, "Unhandled exception", "Unknown exception caught");
        }
    }

    void AddFailure(std::string file, int line, std::string expr, std::string msg) {
        failures_.push_back({std::move(file), line, std::move(expr), std::move(msg)});
    }

    [[nodiscard]] const std::string& suite() const { return suite_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] bool passed() const { return failures_.empty(); }
    [[nodiscard]] const std::vector<TestFailure>& failures() const { return failures_; }

private:
    std::string suite_;
    std::string name_;
    std::function<void()> fn_;
    std::vector<TestFailure> failures_;
};

class TestRegistry {
public:
    static TestRegistry& Instance() {
        static TestRegistry reg;
        return reg;
    }

    void Register(std::string suite, std::string name, std::function<void()> fn) {
        tests_.emplace_back(std::move(suite), std::move(name), std::move(fn));
    }

    void SetCurrentTest(TestCase* tc) { current_test_ = tc; }
    TestCase* CurrentTest() { return current_test_; }

    int RunAll() {
        int passed = 0;
        int failed = 0;
        std::cout << "\n=======================================================\n";
        std::cout << "[==========] Running " << tests_.size() << " test(s).\n";

        auto start_total = std::chrono::high_resolution_clock::now();

        for (auto& tc : tests_) {
            SetCurrentTest(&tc);
            std::cout << "[ RUN      ] " << tc.suite() << "." << tc.name() << "\n";
            auto start = std::chrono::high_resolution_clock::now();
            tc.Run();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count();

            if (tc.passed()) {
                std::cout << "[       OK ] " << tc.suite() << "." << tc.name()
                          << " (" << elapsed << " ms)\n";
                passed++;
            } else {
                std::cout << "[  FAILED  ] " << tc.suite() << "." << tc.name()
                          << " (" << elapsed << " ms)\n";
                for (const auto& fail : tc.failures()) {
                    std::cout << "  " << fail.file << ":" << fail.line
                              << ": Failure in: " << fail.expression << "\n"
                              << "    " << fail.message << "\n";
                }
                failed++;
            }
        }

        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_total).count();

        std::cout << "[==========] " << tests_.size() << " test(s) run (" << total_ms << " ms total).\n";
        std::cout << "[  PASSED  ] " << passed << " test(s).\n";
        if (failed > 0) {
            std::cout << "[  FAILED  ] " << failed << " test(s).\n";
        }
        std::cout << "=======================================================\n\n";

        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
    TestCase* current_test_ = nullptr;
};

struct TestRegistrar {
    TestRegistrar(std::string suite, std::string name, std::function<void()> fn) {
        TestRegistry::Instance().Register(std::move(suite), std::move(name), std::move(fn));
    }
};

} // namespace openroute::test

#define OPENROUTE_TEST_CONCAT_INNER(a, b) a##b
#define OPENROUTE_TEST_CONCAT(a, b) OPENROUTE_TEST_CONCAT_INNER(a, b)

#define TEST(Suite, Name)                                                                       \
    void OPENROUTE_TEST_CONCAT(Suite, OPENROUTE_TEST_CONCAT(_, Name))();                         \
    static ::openroute::test::TestRegistrar OPENROUTE_TEST_CONCAT(registrar_, __LINE__)(       \
        #Suite, #Name, OPENROUTE_TEST_CONCAT(Suite, OPENROUTE_TEST_CONCAT(_, Name)));          \
    void OPENROUTE_TEST_CONCAT(Suite, OPENROUTE_TEST_CONCAT(_, Name))()

#define EXPECT_TRUE(cond)                                                                       \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #cond, "Expected true, got false");       \
        }                                                                                       \
    } while (0)

#define EXPECT_FALSE(cond)                                                                      \
    do {                                                                                        \
        if (cond) {                                                                             \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #cond, "Expected false, got true");      \
        }                                                                                       \
    } while (0)

#define ASSERT_TRUE(cond) EXPECT_TRUE(cond)
#define ASSERT_FALSE(cond) EXPECT_FALSE(cond)
#define ASSERT_EQ(a, b) EXPECT_EQ(a, b)
#define ASSERT_NE(a, b) EXPECT_NE(a, b)

#define EXPECT_EQ(a, b)                                                                         \
    do {                                                                                        \
        auto val_a = (a);                                                                       \
        auto val_b = (b);                                                                       \
        if (!(val_a == val_b)) {                                                                \
            std::ostringstream ss;                                                              \
            ss << "Expected equality: (" << #a << " = " << val_a << ") vs ("                    \
               << #b << " = " << val_b << ")";                                                 \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #a " == " #b, ss.str());                 \
        }                                                                                       \
    } while (0)

#define EXPECT_NE(a, b)                                                                         \
    do {                                                                                        \
        auto val_a = (a);                                                                       \
        auto val_b = (b);                                                                       \
        if (val_a == val_b) {                                                                   \
            std::ostringstream ss;                                                              \
            ss << "Expected inequality but values were equal: " << val_a;                       \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #a " != " #b, ss.str());                 \
        }                                                                                       \
    } while (0)

#define EXPECT_GT(a, b)                                                                         \
    do {                                                                                        \
        auto val_a = (a);                                                                       \
        auto val_b = (b);                                                                       \
        if (!(val_a > val_b)) {                                                                 \
            std::ostringstream ss;                                                              \
            ss << "Expected " << #a << " (" << val_a << ") > " << #b << " (" << val_b << ")"; \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #a " > " #b, ss.str());                  \
        }                                                                                       \
    } while (0)

#define EXPECT_THROW(expr, ExcType)                                                             \
    do {                                                                                        \
        bool caught = false;                                                                    \
        try {                                                                                   \
            expr;                                                                               \
        } catch (const ExcType&) {                                                              \
            caught = true;                                                                      \
        } catch (...) {                                                                         \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #expr, "Caught unexpected exception");   \
        }                                                                                       \
        if (!caught) {                                                                          \
            auto* tc = ::openroute::test::TestRegistry::Instance().CurrentTest();              \
            if (tc) tc->AddFailure(__FILE__, __LINE__, #expr, "Expected exception " #ExcType); \
        }                                                                                       \
    } while (0)
