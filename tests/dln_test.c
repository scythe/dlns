#include "test/unity/src/unity.h"
#include "dln.c"

void test_longstring(void)
{
	struct newline WINNTNL = {'\n', '\r'}, LINUXNL = {'\n', '\0'};
	TEST_ASSERT_EQUAL_INT(longquote(",", WINNTNL, "1234567890.:"), 0);
	//FIXME
}

int main(int argc, char **argv)
{
	UNITY_BEGIN();
	RUN_TEST(test_longstring);
	return UNITY_END();
}

