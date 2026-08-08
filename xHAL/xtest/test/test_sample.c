#include "../xhal_test.h"
#include "test_config.h"

#if TEST_IS_ENABLED(SAMPLE)
/* 定义一个测试组 */
TEST_GROUP(sample);

/* 组的初始化 */
TEST_SETUP(sample)
{
}

/* 组的清理 */
TEST_TEAR_DOWN(sample)
{
}

/* 测试用例AddTwoNumbers */
TEST(sample, AddTwoNumbers)
{
    int a = 2, b = 3;
    TEST_ASSERT_EQUAL_INT(5, a + b);
}

/* 测试用例StringCompare */
TEST(sample, StringCompare)
{
    const char *s1 = "hell0";
    const char *s2 = "hell0";
    TEST_ASSERT_EQUAL_STRING(s1, s2);
}

/* 组的 runner，负责运行上面的用例 */
TEST_GROUP_RUNNER(sample)
{
    RUN_TEST_CASE(sample, AddTwoNumbers);
    RUN_TEST_CASE(sample, StringCompare);
}
#endif