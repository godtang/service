#include "config.h"

// ����ʱ��֤�״ε���ʱ�������̰߳�ȫ���� 
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
