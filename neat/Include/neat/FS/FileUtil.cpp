#include "pch.h"
#include "FileUtil.h"

long long neat::FileAgeDiff(const char* base, const char* ref)
{
	struct _stat result;
	_stat( base, &result );
	const auto lastModifiedBase = result.st_mtime;
	_stat( ref, &result );
	const auto lastModifiedReg = result.st_mtime;

	return lastModifiedBase - lastModifiedReg;
}
