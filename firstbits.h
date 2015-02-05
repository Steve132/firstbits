
#include<cstring>
#include<cstdint>
#include<cctype>
#include<memory>
#include<string>
#include<atomic>
#include<array>

#include<sys/mman.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "microhttpdpp.hpp"

#include "mhttpdfiles.h"
#include "microhttpd.h"

/*
Requests it can recieve: 
	POST /block (authentication required...maybe only allowed from localhost!)
		goes through and processes adding the block to the mmapped file.  Calls msync when finished and then (last) updates the lastblockid number
		(block is not blockfile format (fuck parsing that noise) but is instead a JSON list of addresses and a blocknumber max.
	GET /block
		recieves the hash of the last block added to the DB
		
	GET /firstbits/<bitstring>
		/looks up in the database and returns a list of all the addresses that match..  Needs at least version and 4 characters or it fails.
*/
	

static const unsigned int SIZE_ADDRESS=36;

struct address_t
{
	std::array<char,SIZE_ADDRESS> data;
	typedef std::uint64_t addrmeta_t;
	addrmeta_t meta;
	
	address_t(const std::string& s="",const addrmeta_t& m=0):
		meta(m)
	{
		memset(&data[0],0,SIZE_ADDRESS);
		std::strncpy(&data[0],s.c_str(),std::min((size_t)SIZE_ADDRESS,s.size()));
	}
	
};

//std::istream& operator>>(std::istream& in,address_t::addrmeta_t& meta);
inline std::istream& operator>>(std::istream& in,address_t& addy)
{
	std::string addstr;
	address_t::addrmeta_t amt;
	in >> addstr >> amt;
	addy=address_t(addstr,amt);
	return in;
}

struct firstbits_t
{
public:
	struct metadata_t
	{
		std::uint64_t num_blocks;    //e.g. 36^num_hashsize;
		std::uint16_t max_addresses_per_block; //e.g. 256
		std::uint8_t num_characters_per_block;	  //e.g. 4
	};
	std::uint32_t* lastblockheight;
private:
	char* filebeginning;	//memmaped file
	char* tablebeginning;   //memmapped table;
	

	
	metadata_t mdata;
	mutable std::unique_ptr<std::atomic<std::uint32_t>[] > block_threadstate;
	size_t blocksize;
	size_t pagesize;
	int backendfd;
	
	std::uint64_t hashbucket(const address_t& bitsearch) const;
	void create_file(const std::string& fn,const metadata_t& md);
	int safemsync(void* addr,size_t len,int flags);
public:

	//loading constructor
	firstbits_t(const std::string& filename,const metadata_t& md={36*36*36*36,256,4});

	std::vector<address_t> get_firstbits(const std::string& bitsearch) const;
	
	void insert_address(const address_t& address);
	
	template<class StringIterator>
	void load_block(const uint32_t& blockheight,StringIterator ab,StringIterator ae)
	{
		for(StringIterator ai=ab;ai!=ae;++ai)
		{
			insert_address(*ai);
		}
		*lastblockheight=blockheight;
		//safemsync(lastblockchainid,sizeof(uint32_t),MS_ASYNC);
	}
};