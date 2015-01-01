#ifndef MICROHTTPDPP_H
#define MICROHTTPDPP_H

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include<functional>


typedef std::function<int (const struct sockaddr*,socklen_t)> acceptfunctype;
typedef std::function<
			int (struct MHD_Connection *,
			const std::string&,
			const std::string&,
			const std::string&,
			const char*,
			size_t*,
			void **)
		> respondfunctype;
				
struct MHD_Daemon* mhdpp_start_daemon(		unsigned int flags,
						int port,
						const acceptfunctype& accept,
						const respondfunctype& respond,
						...);

int mdhpp_respond(struct MHD_Connection* connection,const std::string& content,int status=MHD_HTTP_OK,bool must_copy=true);
int mdhpp_default_accept(const struct sockaddr* sa,socklen_t sl);
#endif