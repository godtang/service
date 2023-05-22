#include "config.h"

// 编码时保证首次调用时不存在线程安全问题 
CConfig* CConfig::CreateInstance()
{
	if (nullptr == pInstance)
	{
		pInstance = new CConfig();
	}
	return pInstance;
}

CConfig::CConfig()
{

}
