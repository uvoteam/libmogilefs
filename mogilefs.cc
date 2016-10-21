#include <curl/curl.h>
#include <errno.h>
#include <cstring>

#include <system_error>
#include <string>
#include <stdexcept>
#include <vector>
#include <utility>
#include <sstream>
#include <iomanip>

#include <boost/asio/ip/tcp.hpp>

#include "mogilefs.h"

using boost::asio::ip::tcp;
using namespace std;


namespace
{
	const string OK = "OK ";
	const string NO_KEY = "ERR unknown_key";
	const string PATH = "path=";

    string url_encode(const string &value)
    {
        ostringstream escaped;
        escaped.fill('0');
        escaped << hex;

        for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
        {
            string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }

            // Any other characters are percent-encoded
            escaped << uppercase;
            escaped << '%' << setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            escaped << nouppercase;
        }

        return escaped.str();
    }
}

MogileFS::MogileFS(const vector<pair<string, unsigned int>> &trackers_arg, unsigned int timeout_arg) :
    trackers(trackers_arg), timeout(timeout_arg)
{
}

bool MogileFS::if_key_exists(const string &key, const string &domain)
{
    tcp::iostream stream;		
    connect(stream);
    stream << "GET_PATHS&domain=" + url_encode(domain) + "&key=" + url_encode(key) << endl;		
    if (!stream) throw runtime_error("MogileFS: if_key_exists GET_PATHS error: " + stream.error().message());
    string response;
    getline(stream, response);

    if (!stream) throw runtime_error("MogileFS: if_key_exists getline error:" + stream.error().message());
    if (response.empty()) throw runtime_error("MogileFS: if_key_exists response is empty");
    if (response[response.length() - 1] == '\r') response.pop_back();

    if (!response.compare(0, OK.length(), OK.c_str(), OK.length())) return true;
    if (!response.compare(0, NO_KEY.length(), NO_KEY.c_str(), NO_KEY.length())) return false;
    throw runtime_error(string("MogileFS: if_key_exists wrong response: ") + response);
}

void MogileFS::put_buffer(const string &key, const char *text, const string &domain, const string &tag)
{
    tcp::iostream stream;
    boost::asio::socket_base::linger linger_op(true, 30);
    connect(stream);
    stream.rdbuf()->set_option(linger_op);
    stream << "CREATE_OPEN&domain=" + url_encode(domain) +
        "&class=" + url_encode(tag) +
        "&key=" + url_encode(key) << endl;
    if (!stream) throw runtime_error("MogileFS: if_key_exists CREATE_OPEN write error: " + stream.error().message());
    string response;
    getline(stream, response);
    if (!stream) throw runtime_error("MogileFS: if_key_exists getline error: " + stream.error().message());
    stream.close();
    if (response.empty()) throw runtime_error("MogileFS: put_buffer response is empty");
    if (response[response.length() - 1] == '\r') response.pop_back();
    if (response.compare(0, OK.length(), OK.c_str(), OK.length()))
    {
        throw runtime_error(string("MogileFS: put_buffer wrong response: ") + response);
    }
    size_t url_pos = response.find(PATH);
    if (url_pos == string::npos)
    {
        throw runtime_error(string("MogileFS: put_buffer no ") + PATH + " in response: " + response);
    }
    url_pos += PATH.length();
    size_t amp_pos = response.find('&', url_pos);
    size_t path_len = amp_pos == string::npos? string::npos : amp_pos - url_pos;

    string url = response.substr(url_pos, path_len);
    upload_buf(text, url);

    string close_url = response.substr(OK.length());
    close_url.erase(close_url.find(PATH), path_len == string::npos? string::npos : path_len + PATH.length() + 1);
    if (path_len != string::npos) close_url += '&';
    close_url += PATH + url_encode(url);
    connect(stream);
    stream.rdbuf()->set_option(linger_op);
    stream << "CREATE_CLOSE&key=" << url_encode(key) << '&' << close_url +
        "&domain=" << url_encode(domain) << "&class="<< url_encode(tag) << endl;
    if (!stream) throw runtime_error("MogileFS: if_key_exists CREATE_CLOSE write error: " + stream.error().message());
    stream.close();
}

void MogileFS::connect(tcp::iostream &stream)
{
    for (const pair<string, unsigned int> &tracker : trackers)
    {
        stream.expires_from_now(boost::posix_time::seconds(timeout));
        stream.connect(tracker.first, to_string(tracker.second));

        if (!stream) throw runtime_error(string("Could not connect to server:") + tracker.first +
                                         " port:" + to_string(tracker.second) + " error: " + stream.error().message());
        else return;
        stream.close();
        stream.clear();
    }
    throw runtime_error("Could not connect to any mogilefs tracker");
}

void MogileFS::upload_buf(const char *buf, const string &url)
{
    CURLcode ret;
    CURL *curl;

    curl = curl_easy_init();
    if (curl == NULL) throw runtime_error("curl_easy_init error for mogile");
        
    if ((ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())) != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_setopt_error for mogile: ") + curl_easy_strerror(ret));
    }

    if ((ret = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L)) != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_setopt_error for mogile: ") + curl_easy_strerror(ret));
    }

    if ((ret = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L)) != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_setopt_error for mogile: ") + curl_easy_strerror(ret));
    }

    if ((ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout)) != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_setopt CURL CONTIMEOUT error for mogile: ") + curl_easy_strerror(ret));
    }

    if ((ret = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout)) != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_setopt CURL TIMEOUT error for mogile: ") + curl_easy_strerror(ret));
    }
    size_t buf_len = strlen(buf);
    FILE *file = fmemopen(const_cast<char*>(buf), buf_len, "rb");
    if (!file)
    {
        fclose(file);
        curl_easy_cleanup(curl);
        throw system_error(errno, system_category(), "MogileFS upload_buf: fmemopen error");
    }
        
    if ((ret = curl_easy_setopt(curl, CURLOPT_READDATA, file)) != CURLE_OK)
    {
        fclose(file);
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_setopt CURLOPT_READDATA error for mogile: ") + curl_easy_strerror(ret));
    }
		
    if ((ret = curl_easy_setopt(curl, CURLOPT_INFILESIZE, static_cast<curl_off_t>(buf_len))) != CURLE_OK)
    {
        fclose(file);
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_setopt CURLOPT_INFILESIZE error for mogile: ") + curl_easy_strerror(ret));
    }

    struct curl_slist *chunk = NULL;
    chunk = curl_slist_append(chunk, "Expect:");
    if ((ret = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk)) != CURLE_OK)
    {
        fclose(file);
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_setopt_error CURLOPTHTTPHEADER for mogile ") + curl_easy_strerror(ret));
    }
        
    if ((ret = curl_easy_perform(curl)) != CURLE_OK)
    {
        fclose(file);
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_perform error for mogile: ") + curl_easy_strerror(ret));
    }
    long http_code = 0;
    if ((ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code)) != CURLE_OK)
    {
        fclose(file);
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        throw runtime_error(string("curl_easy_getinfo error for mogile: ") + curl_easy_strerror(ret));
    }

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    fclose(file);
    if (http_code != 201) throw runtime_error(string("mogile answer is not 201. answer= ") + to_string(http_code));
}
