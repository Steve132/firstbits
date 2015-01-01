#ifndef FILECACHE_H
#define FILECACHE_H

#include<string>
#include<cstdlib>
#include<unordered_map>
#include<microhttpd.h>
#include <sys/types.h>
#include <sys/stat.h>

struct filecache
{
	struct filecachevalue_t
	{
		time_t lasttime;
		std::string contents;
	};
	std::unordered_map<std::string,filecachevalue_t> data;
	std::string folderbase;
	
	filecache(const std::string& fb=".");
	int serve_cached_files(struct MHD_Connection * connection,
		    std::string url,
		    const std::string& method,
                    const std::string& version,
		    const char* upload_data,
		    size_t * upload_data_size,
                    void ** ptr);
};

#endif