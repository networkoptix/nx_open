#pragma once

#include <nx/utils/url.h>

namespace nx::vms_server_plugins::analytics::hanwha {

class UrlDecorator
{
public:
	virtual ~UrlDecorator() = default;
	virtual nx::utils::Url decorate(const nx::utils::Url& url) = 0;
};

class MockUrlDecorator : public UrlDecorator
{
public:
	virtual nx::utils::Url decorate(const nx::utils::Url& url) override;
};

class BypassUrlDecorator : public UrlDecorator
{
public:
	BypassUrlDecorator(int channelNumber) : m_channelNumber(channelNumber) {};
	virtual nx::utils::Url decorate(const nx::utils::Url& url) override;

private:
	int m_channelNumber;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
