#pragma once
#include "pcquicklib.h"
class CConfig
{
public:
	CConfig* CreateInstance();
private:
	CConfig();
	CConfig* pInstance = nullptr;
};

