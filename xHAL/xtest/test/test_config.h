#ifndef __TEST_CONFIG_H_
#define __TEST_CONFIG_H_

#define TEST_ENABLE_ALL        1

#define TEST_ENABLE_SAMPLE     1
#define TEST_ENABLE_HASH_TABLE 1

#define TEST_IS_ENABLED(cmd) \
    (XHAL_UNIT_TEST && TEST_ENABLE_ALL && TEST_ENABLE_##cmd)

#endif /* __TEST_CONFIG_H_ */
