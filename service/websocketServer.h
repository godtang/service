#pragma once

#include "pcquicklib.h"

class CWebsocketServer : public CPCWebSocketConnection
{
public:
	void OnClosed() override
	{
		PC_INFO("[0x%p]client closed", this);
	}
	void OnAccepted(const CPCSockAddr& addr) override
	{
		PC_INFO("[0x%p]server accpted :%s", this, addr.GetIPString().c_str());
	}
	CPCResult<> OnHandeShakeOver() override
	{
		PC_INFO("[0x%p]hande shake succ.", this);
		return CPCResult<>();
	}

	CPCResult<> OnRecvText(const std::string& data) override
	{
		PC_INFO("[0x%p]收到 :%s", this, data.c_str());



		this->SendText("hello,world" + data);
		auto xxxx = "hello,world" + data;
		PC_INFO("[0x%p]回应:%s", this, xxxx.c_str());

		return CPCResult<>();
	}
};

