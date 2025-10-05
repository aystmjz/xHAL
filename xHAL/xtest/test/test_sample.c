#include "../../xcore/xhal_assert.h"
#include "../../xcore/xhal_log.h"
#include "../xhal_test.h"

#define EABLE_TEST_GROUP

XLOG_TAG("test_sample");

/* 定义一个测试组 */
TEST_GROUP(sample);

/* 组的初始化 */
TEST_SETUP(sample)
{
    XLOG_DEBUG("\r\nTEST_SETUP start");

    XLOG_DEBUG("TEST_SETUP end");
}

/* 组的清理 */
TEST_TEAR_DOWN(sample)
{
    XLOG_DEBUG("TEST_TEAR_DOWN start");

    XLOG_DEBUG("TEST_TEAR_DOWN end");
}

/* 测试用例AddTwoNumbers */
TEST(sample, AddTwoNumbers)
{
    XLOG_DEBUG("\r\nTEST AddTwoNumbers start");
    int a = 2, b = 3;
    TEST_ASSERT_EQUAL_INT(5, a + b);
    XLOG_DEBUG("TEST AddTwoNumbers end");
}

/* 测试用例StringCompare */
TEST(sample, StringCompare)
{
    XLOG_DEBUG("\r\nTEST StringCompare start");
    const char *s1 = "hello";
    const char *s2 = "hell0";
    TEST_ASSERT_EQUAL_STRING(s1, s2);
    XLOG_DEBUG("TEST StringCompare end");
}

/* 组的 runner，负责运行上面的用例 */
#ifdef EABLE_TEST_GROUP
TEST_GROUP_RUNNER(sample)
{
    RUN_TEST_CASE(sample, AddTwoNumbers);
    RUN_TEST_CASE(sample, StringCompare);
}
#endif