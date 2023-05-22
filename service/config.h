#pragma once

#include "pcquicklib.h"

class CConfig
{
public:
	static CConfig* GetInstance();
	CPCJsonObject Root;
private:
	CConfig();
	static CConfig* pInstance;
	bool Load();
	std::string m_file;
};

