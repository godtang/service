
#include <codecvt>
#include <iostream>

#include "config.h"

CConfig* CConfig::pInstance = nullptr;

// 编码时保证首次调用时不存在线程安全问题 
CConfig* CConfig::GetInstance()
{
	if (nullptr == pInstance)
	{
		pInstance = new CConfig();
		if (!pInstance->Load())
		{
			return nullptr;
		}
	}
	return pInstance;
}

CConfig::CConfig()
{

}

bool CConfig::Load()
{
	auto ret = CPCFileUtil::PCGetSelfFileDir();
	if (!ret)
	{
		OutputDebugString(L"CPCFileUtil::PCGetSelfFileDir() fail");
		return false;
	}
	auto filePath = ret.Get().first + "config.json";
	if (0 != access(filePath.c_str(), F_OK))
	{
		std::string str = "config file not exist, " + filePath;
		std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
		std::wstring wstr = conv.from_bytes(str);;
		OutputDebugString(wstr.c_str());
		return false;
	}

	auto parseFileResult = Root.ParseFile(filePath.c_str());
	if (!parseFileResult)
	{

		std::string str = "parse config file fail, " + parseFileResult.ErrCode();
		str = str + ", " + parseFileResult.ErrDesc();
		std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
		std::wstring wstr = conv.from_bytes(str);;
		OutputDebugString(wstr.c_str());
		return false;
	}

	return true;
}
