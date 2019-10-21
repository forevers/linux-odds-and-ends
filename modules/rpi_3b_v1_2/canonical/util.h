#pragma once

#define PR_INFO(FormatLiteral, ...)     pr_info("%s::(%d): " FormatLiteral "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define PR_ERR(FormatLiteral, ...)      pr_err("%s::(%d): " FormatLiteral "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
