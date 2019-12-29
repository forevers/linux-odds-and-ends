#pragma once

#include <linux/fs.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define PR_INFO(FormatLiteral, ...)     pr_info("%s::%s::(%d): " FormatLiteral "\n", __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define PR_ERR(FormatLiteral, ...)      pr_err("%s::(%d): " FormatLiteral "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
