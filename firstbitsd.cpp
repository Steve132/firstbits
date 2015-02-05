#include<array>
#include<iostream>
#include<atomic>
#include<algorithm>
#include<fstream>
#include<sstream>
#include<memory>
#include<iterator>
#include<thread>
#include<functional>
#include<atomic>
#include "microhttpdpp.hpp"
#include "mhttpdfiles.h"
#include "microhttpd.h"
#include "firstbits.h"

/*
Requests it can recieve: 
	POST API is disabled.
	NAMED PIPE recieves address-int pairs instead.
	POST /api/admin/block/<number> (authentication required...maybe only allowed from localhost!)
		goes through and processes adding the block to the mmapped file.  Calls msync when finished and then (last) updates the lastblockid number
		(block is not blockfile format (fuck parsing that noise) but is instead merely a whitespace separated list of addresses
	POST /api/admin/pay (satoshi)
		Goes through and updates all the given totals from the whitespace seperated address satoshi pairs.  If an address appears twice, the total satoshi are added
	
	GET /api/block
		recieves the hash of the last block added to the DB
		
	GET /api/firstbits/<bitstring>
		/looks up in the database and returns a list of all the addresses that match..  Needs at least version and 4 characters or it fails.
*/
	
#ifdef TESTMODE
int test()
{
	firstbits_t fb("out.db",{36*36*36,16,3});
	const std::string testaddresses[]={"1Applesaucemeister","1Pantywaste","1Appletini","1Appliancedirect","1Applesaucemeister"};
	fb.load_block(3,testaddresses,testaddresses+5);
	auto result=fb.get_firstbits("1Apple");
	
 	for(const address_t* i=result.first;i!=result.second;++i)
	{
		std::cout << &(i->data[0]) << std::endl;
	}
	return 0;
}
#endif

static std::string queryfb(firstbits_t* fb,const std::string& searchquery,bool case_sensitive=false)
{
	std::ostringstream oss;
	oss << "[";	
	bool skip=false;
	for(auto ch:searchquery)
	{
		if(!isalnum(ch))
		{
			skip=true;
		}
	}
	if(!skip)
	{
		auto result=fb->get_firstbits(searchquery);
		bool comma=false;

		for(auto i=result.cbegin();i!=result.cend();++i)
		{
			bool should_skip=case_sensitive && (0!=strncmp(&i->data[0],searchquery.data(),searchquery.size()));
			if(!should_skip)
			{
		
				if(comma)
				{
					oss << ",";
				}
				oss << "\"" << &(i->data[0]) << "\"";
				comma=true;
			}
		}
	}
	
	oss << "]";
	return oss.str();
}

static int api_server(firstbits_t* fb,
			struct MHD_Connection * con,
			const std::string& url,
			const std::string& method,
			const std::string& version,
			const char* upload_data,
			size_t * upload_data_size,
			void ** ptr)
{
	static int dummy;
	static const char* API_FAIL="<html><body><h1>Api Call not recognized</h1></body></html>";
	if(&dummy != *ptr)
	{
		*ptr=&dummy;
		return MHD_YES;
	}
	
	if(method=="GET")
	{
		if(url.substr(5,9)=="firstbits")
		{
			bool case_sensitive;
			std::string::const_iterator ci;
			if(url.substr(15,9)=="sensitive")
			{
				ci=url.cbegin()+25;
				case_sensitive=true;
			}
			else
			{
				ci=url.cbegin()+27;
				case_sensitive=false;
			}
			
			std::string searchquery;
			try
			{
				searchquery=std::string(ci,url.cend());
			} catch(const std::exception& e)
			{
				return mdhpp_respond(con,API_FAIL);
			}
			
			
			return mdhpp_respond(con,queryfb(fb,searchquery,case_sensitive));
		}
		else if(url.substr(5,5)=="block")
		{
			std::ostringstream oss;
			oss << *(fb->lastblockheight) << "\n";
			return mdhpp_respond(con,oss.str());
		}
		return mdhpp_respond(con,API_FAIL);
	}
	else if(method=="POST")
	{
		
		return MHD_NO;
	/*	if(url.substr(5,5)!="admin")
		{
			return MHD_NO;
		}
		const MHD_ConnectionInfo* mci=MHD_get_connection_info(con,MHD_CONNECTION_INFO_CLIENT_ADDRESS);
		//std::uint32_t lhst=htonl(INADDR_LOOPBACK);//only localhost can connect via POST
		const uint8_t lhstb[4]={0x7F,0x00,0x00,0x01};
		if(memcmp(mci->client_addr->sa_data,lhstb,4)!=0)
		{
			return MHD_NO;
		}
		
		std::string postdata(upload_data,upload_data+*upload_data_size);
		std::istringstream input(postdata);
		//	/api/admin/pro/
		//	/api/admin/block/
		//	0123456789ABCDEF01
		if(url.substr(0xB,3)=="pro")
		{
			for(auto ai=std::istream_iterator<address_t>(input);ai!=std::istream_iterator<address_t>();++ai)
			{
				fb->insert_address(*ai);
			}
		}
		else if(url.substr(0xB,5)=="block")
		{
			int bh=atoi(url.c_str()+0x11);
			fb->load_block(bh,std::istream_iterator<std::string>(input),std::istream_iterator<std::string>());
		}
		else
		{
			return MHD_NO;
		}

		return MHD_YES;*/
	}
	return MHD_NO;
}



static int serve_dispatch(filecache* fc,firstbits_t* fb,
			struct MHD_Connection * con,
			const std::string& url,
			const std::string& method,
			const std::string& version,
			const char* upload_data,
			size_t * upload_data_size,
			void ** ptr)
{
#ifndef NDEBUG
	std::cout << method << " " << url << std::endl;
#endif
	if(url.substr(0,4)=="/api")
	{
		return api_server(fb,con,url,method,version,upload_data,upload_data_size,ptr);
	}
	else
	{
		return fc->serve_cached_files(con,url,method,version,upload_data,upload_data_size,ptr);
	}
}

static void update_thread(std::atomic<bool>* bail,firstbits_t* fb,const std::string& namedpipename="firstbitscmds")
{	
	while(!(*bail))
	{
		std::ifstream inpipe(namedpipename.c_str());
		if(!inpipe)
		{
			if(0!=mkfifo(namedpipename.c_str(),0766))
			{
				throw std::runtime_error("Error, failed to make named pipe");
			}
			continue;
		}
		uint32_t bh;	//first print blockheight.
		inpipe >> bh;	//then print blocks
		fb->load_block(bh,std::istream_iterator<address_t>(inpipe),std::istream_iterator<address_t>());
	}
}

int main(int argc,char** argv)
{
	std::string httpdir=".";
	uint32_t httpport=8080;
	std::string dbfile="out.db";
	uint32_t num_characters_per_block=3;
	uint32_t num_addresses_per_block=16;
	uint32_t load_block;
	bool do_load_block=false;
	bool create_only=false;
	
	std::vector<std::string> args(argv,argv+argc);
	for(int i=1;i<argc;i++)
	{
		if(args[i]=="--httpdir")
		{
			httpdir=args[++i];
		}
		else if(args[i]=="--httpport")
		{
			std::istringstream(args[++i]) >> httpport;
		}
		else if(args[i]=="--dbfile")
		{
			dbfile=args[++i];
		}
		else if(args[i]=="--num_characters_per_block")
		{
			std::istringstream(args[++i]) >> num_characters_per_block;
		}
		else if(args[i]=="--num_addresses_per_block")
		{
			std::istringstream(args[++i]) >> num_addresses_per_block;
		}
		else if(args[i]=="--load_block")
		{
			std::istringstream(args[++i]) >> load_block;
			do_load_block=true;
		}
		else if(args[i]=="--create_only")
		{
			create_only=true;
		}
	}
	
	firstbits_t::metadata_t mdata;
	mdata.num_blocks=1;
	mdata.max_addresses_per_block=num_addresses_per_block;
	mdata.num_characters_per_block=num_characters_per_block;
	for(int i=0;i<mdata.num_characters_per_block;i++)
	{
		mdata.num_blocks*=36;
	}
	firstbits_t fb(dbfile,mdata);
	
	if(!do_load_block)
	{
		if(create_only)
		{
			return 0;
		}
		filecache fc(httpdir);
		
		respondfunctype rfn=std::bind(serve_dispatch,&fc,&fb,
					std::placeholders::_1,
					std::placeholders::_2,
					std::placeholders::_3,
					std::placeholders::_4,
					std::placeholders::_5,
					std::placeholders::_6,
					std::placeholders::_7);
  
		struct MHD_Daemon* d = mhdpp_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		       httpport,
		       mdhpp_default_accept,
		       rfn,
		       MHD_OPTION_END);
		if (d == NULL)
			return 1;
		std::atomic<bool> bail;
		bail=false;
		std::thread controlfifothread(update_thread,&bail,&fb,"Hello");
		(void) getchar ();
		bail=true;
		controlfifothread.join();
		MHD_stop_daemon(d);
		return 0;

	}
	else
	{
		fb.load_block(load_block,(std::istream_iterator<std::string>(std::cin)),std::istream_iterator<std::string>());//load blocks from stdin
	}
	return 0;
}
