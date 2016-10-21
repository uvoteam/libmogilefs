#ifndef MOGILEFS_H
#define MOGILEFS_H

#include <vector>
#include <utility>
#include <string>

#include <boost/asio/ip/tcp.hpp>

class MogileFS
{
public:
	MogileFS(const std::vector<std::pair<std::string, unsigned int>> &trackers_arg, unsigned int timeout_arg);
	bool if_key_exists(const std::string &key, const std::string &domain);
	void put_buffer(const std::string &key, const char *text, const std::string &domain, const std::string &tag);

private:
	void connect(boost::asio::ip::tcp::iostream &stream);
	void upload_buf(const char *buf, const std::string &url);

private:
    std::vector<std::pair<std::string, unsigned int>> trackers;
    unsigned int timeout;
};

#endif //MOGILEFS_H
