#include "microhttpdpp.hpp"

extern "C" 
{
	static int accept_cwrap(void * cls,
		    const struct sockaddr* sa,socklen_t sl)
	{
		const acceptfunctype& af=*reinterpret_cast<const acceptfunctype*>(cls);
		return af(sa,sl);
	}
	static int respond_cwrap(void * cls,
		    struct MHD_Connection * connection,
		    const char * url,
		    const char * method,
                    const char * version,
		    const char * upload_data,
		    size_t * upload_data_size,
                    void ** ptr)
	{
		const respondfunctype& rf=*reinterpret_cast<const respondfunctype*>(cls);
		return rf(connection,url,method,version,upload_data,upload_data_size,ptr);
	}
}
		
struct MHD_Daemon* mhdpp_start_daemon(		unsigned int flags,
						int port,
						const acceptfunctype& accept,
						const respondfunctype& respond,
						...)
{
	struct MHD_Daemon *daemon;
	va_list ap;
	va_start (ap, respond);
	daemon = MHD_start_daemon_va (flags, port, &accept_cwrap, 
		const_cast<void*>(reinterpret_cast<const void*>(&accept)), 
		&respond_cwrap, 
		const_cast<void*>(reinterpret_cast<const void*>(&respond)), ap);
	va_end (ap);
	return daemon;
}
		
int mdhpp_respond(struct MHD_Connection* connection,const std::string& content,int status,bool must_copy)
{
	int ret;
	struct MHD_Response * response;
	response = MHD_create_response_from_data(content.size(),
							(void*)content.data(),
							MHD_NO,
							must_copy ? MHD_YES : MHD_NO);
	ret = MHD_queue_response(connection,
					status,
					response);
	MHD_destroy_response(response);
	return ret;
}
int mdhpp_default_accept(const struct sockaddr* sa,socklen_t sl)
{
	return MHD_YES;
}