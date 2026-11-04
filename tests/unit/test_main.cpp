#include "tests/test_framework.hpp"
#include "core/common/logger.hpp"

int main() {
    openroute::common::Logger::Instance().SetMinLevel(openroute::common::LogLevel::Warn);
    return openroute::test::TestRegistry::Instance().RunAll();
}
